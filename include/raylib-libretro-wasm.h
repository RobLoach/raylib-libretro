/**********************************************************************************************
*
*   raylib-libretro-wasm - Load libretro cores compiled as WebAssembly modules.
*
*   Provides an alternative to dylib-based loading: when InitLibretro() receives a path
*   ending in ".wasm", it uses the WebAssembly C API (wasm-c-api) instead of dlopen.
*
*   The WASM module must:
*     - Export all retro_* functions
*     - Export "memory" (the linear memory)
*     - Export "malloc" and "free" (for host-side struct allocation)
*     - Import callbacks from the "env" namespace:
*         env::video_refresh   (i32 data, i32 width, i32 height, i32 pitch) -> ()
*         env::audio_sample    (i32 left, i32 right) -> ()
*         env::audio_sample_batch (i32 data, i32 frames) -> i32
*         env::input_poll      () -> ()
*         env::input_state     (i32 port, i32 device, i32 index, i32 id) -> i32
*         env::environment     (i32 cmd, i32 data) -> i32
*
*   WASM32 struct layouts (4-byte pointers, little-endian) are handled explicitly.
*   Unknown imports receive a no-op stub and a LOG_WARNING.
*
*   LICENSE: zlib/libpng (same as raylib-libretro)
*
*   Copyright (c) 2024 Rob Loach (@RobLoach)
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_WASM_H
#define RAYLIB_LIBRETRO_WASM_H

static bool IsLibretroWasmCore(const char* path);
static bool InitLibretroWasm(const char* path);
static void CloseLibretroWasm(void);

#ifdef RAYLIB_LIBRETRO_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_WASM_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_WASM_IMPLEMENTATION_ONCE

#include <wasm.h>
#include <stdint.h>
#include <string.h>

// Scratch buffer size for returning strings to WASM (directory paths, usernames, etc.)
#define RAYLIB_LIBRETRO_WASM_SCRATCH 512
// Maximum number of imports a WASM core may declare
#define RAYLIB_LIBRETRO_WASM_MAX_IMPORTS 64

typedef struct rLibretroWasmState {
    wasm_engine_t*    engine;
    wasm_store_t*     store;
    wasm_module_t*    module;
    wasm_instance_t*  instance;
    wasm_memory_t*    memory;        // borrowed from exports_vec

    wasm_extern_vec_t exports_vec;   // owned; keeps borrowed func/memory ptrs alive

    // Owned import funcs – kept alive for the instance's lifetime
    wasm_func_t* import_funcs[RAYLIB_LIBRETRO_WASM_MAX_IMPORTS];
    size_t       import_count;

    // Exported libretro functions (borrowed from exports_vec)
    wasm_func_t* fn_retro_api_version;
    wasm_func_t* fn_retro_get_system_info;
    wasm_func_t* fn_retro_get_system_av_info;
    wasm_func_t* fn_retro_init;
    wasm_func_t* fn_retro_deinit;
    wasm_func_t* fn_retro_set_environment;
    wasm_func_t* fn_retro_set_video_refresh;
    wasm_func_t* fn_retro_set_audio_sample;
    wasm_func_t* fn_retro_set_audio_sample_batch;
    wasm_func_t* fn_retro_set_input_poll;
    wasm_func_t* fn_retro_set_input_state;
    wasm_func_t* fn_retro_set_controller_port_device;
    wasm_func_t* fn_retro_reset;
    wasm_func_t* fn_retro_run;
    wasm_func_t* fn_retro_serialize_size;
    wasm_func_t* fn_retro_serialize;
    wasm_func_t* fn_retro_unserialize;
    wasm_func_t* fn_retro_cheat_reset;
    wasm_func_t* fn_retro_cheat_set;
    wasm_func_t* fn_retro_load_game;
    wasm_func_t* fn_retro_load_game_special;
    wasm_func_t* fn_retro_unload_game;
    wasm_func_t* fn_retro_get_region;
    wasm_func_t* fn_retro_get_memory_data;
    wasm_func_t* fn_retro_get_memory_size;

    // Optional allocator exports
    wasm_func_t* fn_malloc;
    wasm_func_t* fn_free;

    // Scratch buffer address in WASM linear memory (for returning strings)
    int32_t scratch_ptr;
} rLibretroWasmState;

static rLibretroWasmState LibretroWasm = {0};

// ============================================================
// Memory helpers
// ============================================================

static inline uint8_t* WasmMem(void) {
    return (uint8_t*)wasm_memory_data(LibretroWasm.memory);
}

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

static int32_t WasmMalloc(uint32_t size) {
    if (!LibretroWasm.fn_malloc) return 0;
    wasm_val_t args[1] = { WASM_I32_VAL((int32_t)size) };
    wasm_val_t res[1]  = { WASM_INIT_VAL };
    wasm_val_vec_t a = { 1, args }, r = { 1, res };
    wasm_func_call(LibretroWasm.fn_malloc, &a, &r);
    return res[0].of.i32;
}

static void WasmFree(int32_t ptr) {
    if (!LibretroWasm.fn_free || !ptr) return;
    wasm_val_t args[1] = { WASM_I32_VAL(ptr) };
    wasm_val_vec_t a = { 1, args }, r = { 0, NULL };
    wasm_func_call(LibretroWasm.fn_free, &a, &r);
}

static int32_t WasmCopyString(const char* s) {
    if (!s) return 0;
    uint32_t len = (uint32_t)(strlen(s) + 1);
    int32_t ptr = WasmMalloc(len);
    if (ptr) memcpy(WasmMem() + ptr, s, len);
    return ptr;
}

// ============================================================
// Functype builder: n_p i32 params, n_r i32 results
// ============================================================

static wasm_functype_t* WasmMakeFuncTypeII(int n_p, int n_r) {
    wasm_valtype_t* ps[8] = {0};
    wasm_valtype_t* rs[4] = {0};
    for (int i = 0; i < n_p && i < 8; i++) ps[i] = wasm_valtype_new(WASM_I32);
    for (int i = 0; i < n_r && i < 4; i++) rs[i] = wasm_valtype_new(WASM_I32);

    wasm_valtype_vec_t pvec, rvec;
    if (n_p > 0) wasm_valtype_vec_new(&pvec, (size_t)n_p, ps);
    else wasm_valtype_vec_new_empty(&pvec);
    if (n_r > 0) wasm_valtype_vec_new(&rvec, (size_t)n_r, rs);
    else wasm_valtype_vec_new_empty(&rvec);

    return wasm_functype_new(&pvec, &rvec); // functype owns pvec/rvec
}

// Build a functype whose param/result kinds mirror the declared import type
static wasm_functype_t* WasmMirrorFuncType(const wasm_functype_t* src) {
    const wasm_valtype_vec_t* sp = wasm_functype_params(src);
    const wasm_valtype_vec_t* sr = wasm_functype_results(src);

    wasm_valtype_t** ps = sp->size ? (wasm_valtype_t**)malloc(sp->size * sizeof(wasm_valtype_t*)) : NULL;
    wasm_valtype_t** rs = sr->size ? (wasm_valtype_t**)malloc(sr->size * sizeof(wasm_valtype_t*)) : NULL;

    for (size_t i = 0; i < sp->size; i++) ps[i] = wasm_valtype_new(wasm_valtype_kind(sp->data[i]));
    for (size_t i = 0; i < sr->size; i++) rs[i] = wasm_valtype_new(wasm_valtype_kind(sr->data[i]));

    wasm_valtype_vec_t pvec, rvec;
    if (sp->size) wasm_valtype_vec_new(&pvec, sp->size, ps);
    else wasm_valtype_vec_new_empty(&pvec);
    if (sr->size) wasm_valtype_vec_new(&rvec, sr->size, rs);
    else wasm_valtype_vec_new_empty(&rvec);

    free(ps);
    free(rs);
    return wasm_functype_new(&pvec, &rvec);
}

// ============================================================
// WASM-aware environment callback
// ============================================================

// LibretroInitCoreVariable, LibretroGetCoreVariable, LibretroInitVideo, LibretroInitAudio,
// LibretroVideoRefresh, LibretroAudioSample, LibretroAudioSampleBatch,
// LibretroInputPoll, LibretroInputState are all defined earlier in raylib-libretro.h.

static bool WasmLibretroEnvironment(unsigned cmd, void* data_host) {
    uint8_t* mem = WasmMem();
    uint8_t* d   = (uint8_t*)data_host; // raw bytes at WASM memory address

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

        // Directory/path queries: return WASM address of scratch buffer (holds cwd)
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
            if (!d) return false;
            WasmWriteU32(d, (uint32_t)LibretroWasm.scratch_ptr);
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            if (!d) return false;
            LibretroCore.pixelFormat = (enum retro_pixel_format)WasmReadU32(d);
            switch (LibretroCore.pixelFormat) {
                case RETRO_PIXEL_FORMAT_0RGB1555: TraceLog(LOG_INFO, "LIBRETRO: WASM: Pixel format 0RGB1555"); break;
                case RETRO_PIXEL_FORMAT_XRGB8888: TraceLog(LOG_INFO, "LIBRETRO: WASM: Pixel format XRGB8888"); break;
                case RETRO_PIXEL_FORMAT_RGB565:   TraceLog(LOG_INFO, "LIBRETRO: WASM: Pixel format RGB565");   break;
                default:
                    TraceLog(LOG_ERROR, "LIBRETRO: WASM: Unknown pixel format, falling to RGB565");
                    LibretroCore.pixelFormat = RETRO_PIXEL_FORMAT_RGB565;
                    return false;
            }
            return true;

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            // WASM32: struct retro_variable { uint32_t key, uint32_t value }
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
            WasmWriteU32(d + 4, (uint32_t)LibretroWasm.scratch_ptr);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            // WASM32: array of { uint32_t key, uint32_t value } terminated by {0, 0}
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
                entry += 8; // advance to next {key, value} pair
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
            // WASM32 layout: geometry{u32×4,f32}(+0..+19), 4 bytes pad, timing{f64,f64}(+24..+39)
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
            // WASM32: retro_game_geometry { u32 base_w, u32 base_h, u32 max_w, u32 max_h, f32 aspect }
            if (!d) return false;
            LibretroCore.width  = WasmReadU32(d + 0);
            LibretroCore.height = WasmReadU32(d + 4);
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
                WasmWriteU32(d, (uint32_t)LibretroWasm.scratch_ptr);
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
// Host callback functions (registered as WASM imports)
// ============================================================

static wasm_trap_t* WasmCbVideoRefresh(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    int32_t data_off = args->data[0].of.i32;
    int32_t width    = args->data[1].of.i32;
    int32_t height   = args->data[2].of.i32;
    int32_t pitch    = args->data[3].of.i32;
    const void* data = data_off ? WasmMem() + data_off : NULL;
    LibretroVideoRefresh(data, (unsigned)width, (unsigned)height, (size_t)pitch);
    (void)env; (void)results;
    return NULL;
}

static wasm_trap_t* WasmCbAudioSample(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    int16_t left  = (int16_t)args->data[0].of.i32;
    int16_t right = (int16_t)args->data[1].of.i32;
    LibretroAudioSample(left, right);
    (void)env; (void)results;
    return NULL;
}

static wasm_trap_t* WasmCbAudioSampleBatch(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    int32_t data_off = args->data[0].of.i32;
    int32_t frames   = args->data[1].of.i32;
    const int16_t* data = data_off ? (const int16_t*)(WasmMem() + data_off) : NULL;
    size_t written = data ? LibretroAudioSampleBatch(data, (size_t)frames) : 0;
    results->data[0].of.i32 = (int32_t)written;
    (void)env;
    return NULL;
}

static wasm_trap_t* WasmCbInputPoll(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    LibretroInputPoll();
    (void)env; (void)args; (void)results;
    return NULL;
}

static wasm_trap_t* WasmCbInputState(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    unsigned port   = (unsigned)args->data[0].of.i32;
    unsigned device = (unsigned)args->data[1].of.i32;
    unsigned index  = (unsigned)args->data[2].of.i32;
    unsigned id     = (unsigned)args->data[3].of.i32;
    results->data[0].of.i32 = (int32_t)LibretroInputState(port, device, index, id);
    (void)env;
    return NULL;
}

static wasm_trap_t* WasmCbEnvironment(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    unsigned cmd      = (unsigned)args->data[0].of.i32;
    int32_t  data_off = args->data[1].of.i32;
    void* data_host   = data_off ? WasmMem() + data_off : NULL;
    bool ret = WasmLibretroEnvironment(cmd, data_host);
    results->data[0].of.i32 = ret ? 1 : 0;
    (void)env;
    return NULL;
}

// No-op stub for unrecognised imports
static wasm_trap_t* WasmCbStub(void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    for (size_t i = 0; i < results->size; i++) results->data[i].of.i64 = 0;
    (void)env; (void)args;
    return NULL;
}

// ============================================================
// Import setup
// ============================================================

static bool WasmNameIs(const wasm_name_t* name, const char* str) {
    size_t len = strlen(str);
    return name->size == len && memcmp(name->data, str, len) == 0;
}

static bool WasmBuildImports(wasm_store_t* store, wasm_module_t* module,
                              wasm_extern_vec_t* out_imports) {
    wasm_importtype_vec_t import_types;
    wasm_module_imports(module, &import_types);

    if (import_types.size > RAYLIB_LIBRETRO_WASM_MAX_IMPORTS) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Too many imports (%zu > %d)",
            import_types.size, RAYLIB_LIBRETRO_WASM_MAX_IMPORTS);
        wasm_importtype_vec_delete(&import_types);
        return false;
    }

    wasm_extern_t** externs = (wasm_extern_t**)MemAlloc(import_types.size * sizeof(wasm_extern_t*));
    LibretroWasm.import_count = 0;

    for (size_t i = 0; i < import_types.size; i++) {
        const wasm_importtype_t* itype = import_types.data[i];
        const wasm_name_t* iname       = wasm_importtype_name(itype);
        const wasm_externtype_t* etype = wasm_importtype_type(itype);

        if (wasm_externtype_kind(etype) != WASM_EXTERN_FUNC) {
            TraceLog(LOG_WARNING, "LIBRETRO: WASM: Non-function import ignored: %.*s",
                (int)iname->size, iname->data);
            // Provide NULL – instantiation will fail; that's acceptable for non-func imports
            externs[i] = NULL;
            continue;
        }

        const wasm_functype_t* declared_ft = wasm_externtype_as_functype_const(etype);
        wasm_func_callback_with_env_t cb = NULL;

        if      (WasmNameIs(iname, "video_refresh"))        cb = WasmCbVideoRefresh;
        else if (WasmNameIs(iname, "audio_sample"))         cb = WasmCbAudioSample;
        else if (WasmNameIs(iname, "audio_sample_batch"))   cb = WasmCbAudioSampleBatch;
        else if (WasmNameIs(iname, "input_poll"))           cb = WasmCbInputPoll;
        else if (WasmNameIs(iname, "input_state"))          cb = WasmCbInputState;
        else if (WasmNameIs(iname, "environment"))          cb = WasmCbEnvironment;
        else {
            const wasm_name_t* imod = wasm_importtype_module(itype);
            TraceLog(LOG_WARNING, "LIBRETRO: WASM: Unknown import %.*s::%.*s – using stub",
                (int)imod->size, imod->data, (int)iname->size, iname->data);
            cb = WasmCbStub;
        }

        wasm_functype_t* ft = WasmMirrorFuncType(declared_ft);
        wasm_func_t* fn     = wasm_func_new_with_env(store, ft, cb, NULL, NULL);
        wasm_functype_delete(ft);

        if (!fn) {
            TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to create host function for import %.*s",
                (int)iname->size, iname->data);
            MemFree(externs);
            wasm_importtype_vec_delete(&import_types);
            return false;
        }

        LibretroWasm.import_funcs[LibretroWasm.import_count++] = fn;
        externs[i] = wasm_func_as_extern(fn);
    }

    out_imports->size = import_types.size;
    out_imports->data = externs;

    wasm_importtype_vec_delete(&import_types);
    return true;
}

// ============================================================
// Export lookup
// ============================================================

static void WasmFindExports(wasm_module_t* module, wasm_instance_t* instance) {
    wasm_exporttype_vec_t export_types;
    wasm_module_exports(module, &export_types);
    wasm_instance_exports(instance, &LibretroWasm.exports_vec);

    for (size_t i = 0; i < export_types.size && i < LibretroWasm.exports_vec.size; i++) {
        const wasm_name_t* ename = wasm_exporttype_name(export_types.data[i]);
        wasm_extern_t* ext = LibretroWasm.exports_vec.data[i];
        wasm_externkind_t kind = wasm_extern_kind(ext);

        if (kind == WASM_EXTERN_MEMORY) {
            LibretroWasm.memory = wasm_extern_as_memory(ext);
            continue;
        }
        if (kind != WASM_EXTERN_FUNC) continue;

        wasm_func_t* fn = wasm_extern_as_func(ext);

#define MATCH(sym) (WasmNameIs(ename, #sym))
        if      (MATCH(retro_api_version))               LibretroWasm.fn_retro_api_version = fn;
        else if (MATCH(retro_get_system_info))           LibretroWasm.fn_retro_get_system_info = fn;
        else if (MATCH(retro_get_system_av_info))        LibretroWasm.fn_retro_get_system_av_info = fn;
        else if (MATCH(retro_init))                      LibretroWasm.fn_retro_init = fn;
        else if (MATCH(retro_deinit))                    LibretroWasm.fn_retro_deinit = fn;
        else if (MATCH(retro_set_environment))           LibretroWasm.fn_retro_set_environment = fn;
        else if (MATCH(retro_set_video_refresh))         LibretroWasm.fn_retro_set_video_refresh = fn;
        else if (MATCH(retro_set_audio_sample))          LibretroWasm.fn_retro_set_audio_sample = fn;
        else if (MATCH(retro_set_audio_sample_batch))    LibretroWasm.fn_retro_set_audio_sample_batch = fn;
        else if (MATCH(retro_set_input_poll))            LibretroWasm.fn_retro_set_input_poll = fn;
        else if (MATCH(retro_set_input_state))           LibretroWasm.fn_retro_set_input_state = fn;
        else if (MATCH(retro_set_controller_port_device)) LibretroWasm.fn_retro_set_controller_port_device = fn;
        else if (MATCH(retro_reset))                     LibretroWasm.fn_retro_reset = fn;
        else if (MATCH(retro_run))                       LibretroWasm.fn_retro_run = fn;
        else if (MATCH(retro_serialize_size))            LibretroWasm.fn_retro_serialize_size = fn;
        else if (MATCH(retro_serialize))                 LibretroWasm.fn_retro_serialize = fn;
        else if (MATCH(retro_unserialize))               LibretroWasm.fn_retro_unserialize = fn;
        else if (MATCH(retro_cheat_reset))               LibretroWasm.fn_retro_cheat_reset = fn;
        else if (MATCH(retro_cheat_set))                 LibretroWasm.fn_retro_cheat_set = fn;
        else if (MATCH(retro_load_game))                 LibretroWasm.fn_retro_load_game = fn;
        else if (MATCH(retro_load_game_special))         LibretroWasm.fn_retro_load_game_special = fn;
        else if (MATCH(retro_unload_game))               LibretroWasm.fn_retro_unload_game = fn;
        else if (MATCH(retro_get_region))                LibretroWasm.fn_retro_get_region = fn;
        else if (MATCH(retro_get_memory_data))           LibretroWasm.fn_retro_get_memory_data = fn;
        else if (MATCH(retro_get_memory_size))           LibretroWasm.fn_retro_get_memory_size = fn;
        else if (MATCH(malloc))                          LibretroWasm.fn_malloc = fn;
        else if (MATCH(free))                            LibretroWasm.fn_free = fn;
#undef MATCH
    }

    wasm_exporttype_vec_delete(&export_types);
}

// ============================================================
// C wrappers for LibretroCore function pointers
// ============================================================

// Helper: call a void WASM func with no args
static inline void WasmCall0(wasm_func_t* fn) {
    if (!fn) return;
    wasm_val_vec_t a = { 0, NULL }, r = { 0, NULL };
    wasm_func_call(fn, &a, &r);
}

// Helper: call a WASM func with one i32 arg, no result
static inline void WasmCall1i(wasm_func_t* fn, int32_t a0) {
    if (!fn) return;
    wasm_val_t args[1] = { WASM_I32_VAL(a0) };
    wasm_val_vec_t a = { 1, args }, r = { 0, NULL };
    wasm_func_call(fn, &a, &r);
}

// Helper: call a WASM func with two i32 args, no result
static inline void WasmCall2i(wasm_func_t* fn, int32_t a0, int32_t a1) {
    if (!fn) return;
    wasm_val_t args[2] = { WASM_I32_VAL(a0), WASM_I32_VAL(a1) };
    wasm_val_vec_t a = { 2, args }, r = { 0, NULL };
    wasm_func_call(fn, &a, &r);
}

// Helper: call a WASM func with one i32 arg, return i32
static inline int32_t WasmCall1i_ri(wasm_func_t* fn, int32_t a0) {
    if (!fn) return 0;
    wasm_val_t args[1] = { WASM_I32_VAL(a0) };
    wasm_val_t res[1]  = { WASM_INIT_VAL };
    wasm_val_vec_t a = { 1, args }, r = { 1, res };
    wasm_func_call(fn, &a, &r);
    return res[0].of.i32;
}

// Helper: call a WASM func with two i32 args, return i32
static inline int32_t WasmCall2i_ri(wasm_func_t* fn, int32_t a0, int32_t a1) {
    if (!fn) return 0;
    wasm_val_t args[2] = { WASM_I32_VAL(a0), WASM_I32_VAL(a1) };
    wasm_val_t res[1]  = { WASM_INIT_VAL };
    wasm_val_vec_t a = { 2, args }, r = { 1, res };
    wasm_func_call(fn, &a, &r);
    return res[0].of.i32;
}

// Helper: call with no args, return i32
static inline int32_t WasmCall0_ri(wasm_func_t* fn) {
    if (!fn) return 0;
    wasm_val_t res[1] = { WASM_INIT_VAL };
    wasm_val_vec_t a = { 0, NULL }, r = { 1, res };
    wasm_func_call(fn, &a, &r);
    return res[0].of.i32;
}

// --- Wrapper implementations ---

static void WasmRetroInit(void)     { WasmCall0(LibretroWasm.fn_retro_init); }
static void WasmRetroDeinit(void)   { WasmCall0(LibretroWasm.fn_retro_deinit); }
static void WasmRetroReset(void)    { WasmCall0(LibretroWasm.fn_retro_reset); }
static void WasmRetroRun(void)      { WasmCall0(LibretroWasm.fn_retro_run); }
static void WasmRetroCheatReset(void) { WasmCall0(LibretroWasm.fn_retro_cheat_reset); }
static void WasmRetroUnloadGame(void) { WasmCall0(LibretroWasm.fn_retro_unload_game); }

static unsigned WasmRetroApiVersion(void) {
    return (unsigned)WasmCall0_ri(LibretroWasm.fn_retro_api_version);
}

static unsigned WasmRetroGetRegion(void) {
    return (unsigned)WasmCall0_ri(LibretroWasm.fn_retro_get_region);
}

static size_t WasmRetroSerializeSize(void) {
    return (size_t)(uint32_t)WasmCall0_ri(LibretroWasm.fn_retro_serialize_size);
}

static void WasmRetroSetEnvironment(retro_environment_t cb) {
    // Callbacks are provided via WASM imports; this call is a no-op for import-based cores.
    WasmCall1i(LibretroWasm.fn_retro_set_environment, 0);
    (void)cb;
}

static void WasmRetroSetVideoRefresh(retro_video_refresh_t cb) {
    WasmCall1i(LibretroWasm.fn_retro_set_video_refresh, 0);
    (void)cb;
}

static void WasmRetroSetAudioSample(retro_audio_sample_t cb) {
    WasmCall1i(LibretroWasm.fn_retro_set_audio_sample, 0);
    (void)cb;
}

static void WasmRetroSetAudioSampleBatch(retro_audio_sample_batch_t cb) {
    WasmCall1i(LibretroWasm.fn_retro_set_audio_sample_batch, 0);
    (void)cb;
}

static void WasmRetroSetInputPoll(retro_input_poll_t cb) {
    WasmCall1i(LibretroWasm.fn_retro_set_input_poll, 0);
    (void)cb;
}

static void WasmRetroSetInputState(retro_input_state_t cb) {
    WasmCall1i(LibretroWasm.fn_retro_set_input_state, 0);
    (void)cb;
}

static void WasmRetroSetControllerPortDevice(unsigned port, unsigned device) {
    WasmCall2i(LibretroWasm.fn_retro_set_controller_port_device, (int32_t)port, (int32_t)device);
}

static void WasmRetroGetSystemInfo(struct retro_system_info* info) {
    if (!info || !LibretroWasm.fn_retro_get_system_info) return;
    // WASM32 retro_system_info: {ptr(+0), ptr(+4), ptr(+8), bool(+12), bool(+13)} ~16 bytes
    int32_t wptr = WasmMalloc(16);
    if (!wptr) return;
    uint8_t* mem = WasmMem();
    memset(mem + wptr, 0, 16);
    WasmCall1i(LibretroWasm.fn_retro_get_system_info, wptr);
    uint32_t name_ptr = WasmReadU32(mem + wptr + 0);
    uint32_t ver_ptr  = WasmReadU32(mem + wptr + 4);
    uint32_t ext_ptr  = WasmReadU32(mem + wptr + 8);
    info->library_name     = name_ptr ? (const char*)(mem + name_ptr) : "";
    info->library_version  = ver_ptr  ? (const char*)(mem + ver_ptr)  : "";
    info->valid_extensions = ext_ptr  ? (const char*)(mem + ext_ptr)  : "";
    info->need_fullpath    = mem[wptr + 12] != 0;
    info->block_extract    = mem[wptr + 13] != 0;
    WasmFree(wptr);
}

static void WasmRetroGetSystemAvInfo(struct retro_system_av_info* info) {
    if (!info || !LibretroWasm.fn_retro_get_system_av_info) return;
    // WASM32 retro_system_av_info: geometry(+0..+19) + pad(4) + timing{f64,f64}(+24..+39) = 40 bytes
    int32_t wptr = WasmMalloc(40);
    if (!wptr) return;
    uint8_t* mem = WasmMem();
    memset(mem + wptr, 0, 40);
    WasmCall1i(LibretroWasm.fn_retro_get_system_av_info, wptr);
    info->geometry.base_width   = WasmReadU32(mem + wptr + 0);
    info->geometry.base_height  = WasmReadU32(mem + wptr + 4);
    info->geometry.max_width    = WasmReadU32(mem + wptr + 8);
    info->geometry.max_height   = WasmReadU32(mem + wptr + 12);
    info->geometry.aspect_ratio = WasmReadF32(mem + wptr + 16);
    info->timing.fps            = WasmReadF64(mem + wptr + 24);
    info->timing.sample_rate    = WasmReadF64(mem + wptr + 32);
    WasmFree(wptr);
}

static bool WasmRetroLoadGame(const struct retro_game_info* game) {
    if (!LibretroWasm.fn_retro_load_game) return false;
    if (!game) {
        // Content-less core
        return WasmCall1i_ri(LibretroWasm.fn_retro_load_game, 0) != 0;
    }
    // WASM32 retro_game_info: {ptr(+0), ptr(+4), u32(+8), ptr(+12)} = 16 bytes
    int32_t game_wptr = WasmMalloc(16);
    if (!game_wptr) return false;
    uint8_t* mem = WasmMem();
    memset(mem + game_wptr, 0, 16);

    int32_t path_wptr = 0, data_wptr = 0;
    if (game->path) {
        path_wptr = WasmCopyString(game->path);
    }
    if (game->data && game->size > 0) {
        data_wptr = WasmMalloc((uint32_t)game->size);
        if (data_wptr) memcpy(mem + data_wptr, game->data, game->size);
    }
    WasmWriteU32(mem + game_wptr + 0,  (uint32_t)path_wptr);
    WasmWriteU32(mem + game_wptr + 4,  (uint32_t)data_wptr);
    WasmWriteU32(mem + game_wptr + 8,  (uint32_t)game->size);
    WasmWriteU32(mem + game_wptr + 12, 0); // meta = NULL

    int32_t ok = WasmCall1i_ri(LibretroWasm.fn_retro_load_game, game_wptr);

    if (path_wptr) WasmFree(path_wptr);
    if (data_wptr) WasmFree(data_wptr);
    WasmFree(game_wptr);
    return ok != 0;
}

static bool WasmRetroLoadGameSpecial(unsigned type, const struct retro_game_info* infos, size_t num) {
    // Not implemented for WASM – use retro_load_game instead
    (void)type; (void)infos; (void)num;
    return false;
}

static bool WasmRetroSerialize(void* data, size_t size) {
    if (!LibretroWasm.fn_retro_serialize || !data || !size) return false;
    int32_t wptr = WasmMalloc((uint32_t)size);
    if (!wptr) return false;
    memset(WasmMem() + wptr, 0, size);
    int32_t ok = WasmCall2i_ri(LibretroWasm.fn_retro_serialize, wptr, (int32_t)size);
    if (ok) memcpy(data, WasmMem() + wptr, size);
    WasmFree(wptr);
    return ok != 0;
}

static bool WasmRetroUnserialize(const void* data, size_t size) {
    if (!LibretroWasm.fn_retro_unserialize || !data || !size) return false;
    int32_t wptr = WasmMalloc((uint32_t)size);
    if (!wptr) return false;
    memcpy(WasmMem() + wptr, data, size);
    int32_t ok = WasmCall2i_ri(LibretroWasm.fn_retro_unserialize, wptr, (int32_t)size);
    WasmFree(wptr);
    return ok != 0;
}

static void WasmRetroCheatSet(unsigned index, bool enabled, const char* code) {
    if (!LibretroWasm.fn_retro_cheat_set) return;
    int32_t code_wptr = WasmCopyString(code ? code : "");
    wasm_val_t args[3] = {
        WASM_I32_VAL((int32_t)index),
        WASM_I32_VAL(enabled ? 1 : 0),
        WASM_I32_VAL(code_wptr)
    };
    wasm_val_vec_t a = { 3, args }, r = { 0, NULL };
    wasm_func_call(LibretroWasm.fn_retro_cheat_set, &a, &r);
    WasmFree(code_wptr);
}

static void* WasmRetroGetMemoryData(unsigned id) {
    if (!LibretroWasm.fn_retro_get_memory_data) return NULL;
    int32_t ptr = WasmCall1i_ri(LibretroWasm.fn_retro_get_memory_data, (int32_t)id);
    return ptr ? WasmMem() + ptr : NULL;
}

static size_t WasmRetroGetMemorySize(unsigned id) {
    if (!LibretroWasm.fn_retro_get_memory_size) return 0;
    return (size_t)(uint32_t)WasmCall1i_ri(LibretroWasm.fn_retro_get_memory_size, (int32_t)id);
}

// ============================================================
// Wire LibretroCore function pointers to WASM wrappers
// ============================================================

static void WasmWireCoreFuncs(void) {
    LibretroCore.retro_init                    = WasmRetroInit;
    LibretroCore.retro_deinit                  = WasmRetroDeinit;
    LibretroCore.retro_api_version             = WasmRetroApiVersion;
    LibretroCore.retro_set_environment         = WasmRetroSetEnvironment;
    LibretroCore.retro_set_video_refresh       = WasmRetroSetVideoRefresh;
    LibretroCore.retro_set_audio_sample        = WasmRetroSetAudioSample;
    LibretroCore.retro_set_audio_sample_batch  = WasmRetroSetAudioSampleBatch;
    LibretroCore.retro_set_input_poll          = WasmRetroSetInputPoll;
    LibretroCore.retro_set_input_state         = WasmRetroSetInputState;
    LibretroCore.retro_get_system_info         = WasmRetroGetSystemInfo;
    LibretroCore.retro_get_system_av_info      = WasmRetroGetSystemAvInfo;
    LibretroCore.retro_set_controller_port_device = WasmRetroSetControllerPortDevice;
    LibretroCore.retro_reset                   = WasmRetroReset;
    LibretroCore.retro_run                     = WasmRetroRun;
    LibretroCore.retro_serialize_size          = WasmRetroSerializeSize;
    LibretroCore.retro_serialize               = WasmRetroSerialize;
    LibretroCore.retro_unserialize             = WasmRetroUnserialize;
    LibretroCore.retro_cheat_reset             = WasmRetroCheatReset;
    LibretroCore.retro_cheat_set               = WasmRetroCheatSet;
    LibretroCore.retro_load_game               = WasmRetroLoadGame;
    LibretroCore.retro_load_game_special       = WasmRetroLoadGameSpecial;
    LibretroCore.retro_unload_game             = WasmRetroUnloadGame;
    LibretroCore.retro_get_region              = WasmRetroGetRegion;
    LibretroCore.retro_get_memory_data         = WasmRetroGetMemoryData;
    LibretroCore.retro_get_memory_size         = WasmRetroGetMemorySize;
}

// ============================================================
// Public API
// ============================================================

static bool IsLibretroWasmCore(const char* path) {
    if (!path) return false;
    const char* ext = GetFileExtension(path);
    return TextIsEqual(ext, ".wasm");
}

static bool InitLibretroWasm(const char* path) {
    // Read the .wasm binary
    int file_size = 0;
    unsigned char* file_data = LoadFileData(path, &file_size);
    if (!file_data || file_size <= 0) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to read %s", path);
        return false;
    }

    LibretroWasm.engine = wasm_engine_new();
    if (!LibretroWasm.engine) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to create engine");
        UnloadFileData(file_data);
        return false;
    }

    LibretroWasm.store = wasm_store_new(LibretroWasm.engine);
    if (!LibretroWasm.store) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to create store");
        UnloadFileData(file_data);
        CloseLibretroWasm();
        return false;
    }

    wasm_byte_vec_t binary = { (size_t)file_size, (wasm_byte_t*)file_data };
    LibretroWasm.module = wasm_module_new(LibretroWasm.store, &binary);
    UnloadFileData(file_data);

    if (!LibretroWasm.module) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to compile %s", path);
        CloseLibretroWasm();
        return false;
    }

    wasm_extern_vec_t imports = { 0, NULL };
    if (!WasmBuildImports(LibretroWasm.store, LibretroWasm.module, &imports)) {
        CloseLibretroWasm();
        return false;
    }

    wasm_trap_t* trap = NULL;
    LibretroWasm.instance = wasm_instance_new(LibretroWasm.store, LibretroWasm.module,
                                               (const wasm_extern_vec_t*)&imports, &trap);
    MemFree(imports.data); // Free the pointer array (elements stay alive via import_funcs[])

    if (trap) {
        wasm_message_t msg = { 0, NULL };
        wasm_trap_message(trap, &msg);
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Instantiation trap: %.*s",
            (int)msg.size, msg.data ? msg.data : (wasm_byte_t*)"(no message)");
        wasm_byte_vec_delete(&msg);
        wasm_trap_delete(trap);
        CloseLibretroWasm();
        return false;
    }
    if (!LibretroWasm.instance) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to instantiate %s", path);
        CloseLibretroWasm();
        return false;
    }

    WasmFindExports(LibretroWasm.module, LibretroWasm.instance);

    if (!LibretroWasm.memory) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Module must export 'memory'");
        CloseLibretroWasm();
        return false;
    }
    if (!LibretroWasm.fn_malloc || !LibretroWasm.fn_free) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Module must export 'malloc' and 'free'");
        CloseLibretroWasm();
        return false;
    }
    if (!LibretroWasm.fn_retro_init || !LibretroWasm.fn_retro_run) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Module missing required retro_* exports");
        CloseLibretroWasm();
        return false;
    }

    // Allocate scratch buffer in WASM memory for returning strings
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

    // Wire LibretroCore function pointers to WASM wrappers
    WasmWireCoreFuncs();

    // Use a sentinel so IsLibretroReady() returns true
    LibretroCore.handle = (void*)LibretroWasm.instance;

    TraceLog(LOG_INFO, "LIBRETRO: WASM: Loaded %s", path);
    return true;
}

static void CloseLibretroWasm(void) {
    if (LibretroWasm.scratch_ptr && LibretroWasm.fn_free) {
        WasmFree(LibretroWasm.scratch_ptr);
        LibretroWasm.scratch_ptr = 0;
    }

    if (LibretroWasm.exports_vec.data) {
        wasm_extern_vec_delete(&LibretroWasm.exports_vec);
    }

    // Delete owned import funcs
    for (size_t i = 0; i < LibretroWasm.import_count; i++) {
        if (LibretroWasm.import_funcs[i]) {
            wasm_func_delete(LibretroWasm.import_funcs[i]);
        }
    }
    LibretroWasm.import_count = 0;

    if (LibretroWasm.instance) { wasm_instance_delete(LibretroWasm.instance); LibretroWasm.instance = NULL; }
    if (LibretroWasm.module)   { wasm_module_delete(LibretroWasm.module);     LibretroWasm.module   = NULL; }
    if (LibretroWasm.store)    { wasm_store_delete(LibretroWasm.store);       LibretroWasm.store    = NULL; }
    if (LibretroWasm.engine)   { wasm_engine_delete(LibretroWasm.engine);     LibretroWasm.engine   = NULL; }

    LibretroWasm.memory = NULL;
    LibretroWasm.fn_malloc = NULL;
    LibretroWasm.fn_free   = NULL;
}

#endif // RAYLIB_LIBRETRO_WASM_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_IMPLEMENTATION
#endif // RAYLIB_LIBRETRO_WASM_H
