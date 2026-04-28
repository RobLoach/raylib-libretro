/**********************************************************************************************
*
*   raylib-libretro-shaders.h - Retro shader library for raylib.
*
*   USAGE:
*       #define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
*       #include "raylib-libretro-shaders.h"
*
*   GLSL version is selected automatically based on PLATFORM_DESKTOP.
*   Override with: #define GLSL_VERSION 100 / 120 / 330
*
*   LICENSE: zlib/libpng
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_SHADERS_H
#define RAYLIB_LIBRETRO_SHADERS_H

#include "raylib.h"

#if !defined(GLSL_VERSION)
    #if defined(PLATFORM_DESKTOP)
        #define GLSL_VERSION 330
    #else
        #define GLSL_VERSION 100
    #endif
#endif

 /**
  * Shader Type
  */
typedef enum LibretroShaderType {
    LIBRETRO_SHADER_NONE             = 0,    // Pass-through, no post-processing
    LIBRETRO_SHADER_CRT              = 1,    // CRT monitor: barrel distortion, scanlines
    LIBRETRO_SHADER_SCANLINES        = 2,    // Lightweight horizontal scanline overlay
    LIBRETRO_SHADER_VIGNETTE         = 3,    // Darkened oval corners
    LIBRETRO_SHADER_GRAYSCALE        = 4,    // Monochrome with optional tint
    LIBRETRO_SHADER_NTSC             = 5,    // NTSC composite: chroma bleed, dot crawl
    LIBRETRO_SHADER_XBRZ             = 6,    // xBRZ 4x edge-directed upscale
    LIBRETRO_SHADER_TYPE_COUNT       = 7
} LibretroShaderType;

#define RAYLIB_LIBRETRO_SHADERS_MAX (LIBRETRO_SHADER_TYPE_COUNT - 1)

typedef struct ShaderCRTParams {
    float brightness;           // Overall brightness multiplier  (default: 1.2)
    float scanlineIntensity;    // Dot mask spacing intensity     (default: 2.0)
    float curvatureRadius;      // Barrel distortion strength     (default: 0.4)
    float cornerSize;           // Rounded corner radius percent  (default: 5.0)
    float cornerSmooth;         // Corner anti-alias softness     (default: 35.0)
    int   curvature;            // Enable barrel distortion       (default: 1)
    int   border;               // Enable corner vignette/border  (default: 1)
    int loc_resolution;
    int loc_brightness;
    int loc_scanlineIntensity;
    int loc_curvatureRadius;
    int loc_cornerSize;
    int loc_cornerSmooth;
    int loc_curvature;
    int loc_border;
} ShaderCRTParams;

typedef struct ShaderScanlinesParams {
    float frequency;    // Lines per screen height       (default: 150.0)
    float opacity;      // Darkness of the dark bands    (default: 0.5)
    float offset;       // Vertical scroll offset (0..1) (default: 0.0)
    float time;         // Accumulated time — internal
    int loc_frequency;
    int loc_opacity;
    int loc_offset;
    int loc_time;
} ShaderScanlinesParams;

typedef struct ShaderVignetteParams {
    float radius;        // Bright inner area radius (default: 0.75)
    float softness;      // Falloff softness         (default: 0.45)
    float opacity;       // Max corner darkening     (default: 0.6)
    Vector3 tintColor;   // Shadow color             (default: {0, 0, 0})
    int loc_radius;
    int loc_softness;
    int loc_opacity;
    int loc_tintColor;
} ShaderVignetteParams;

typedef struct ShaderGrayscaleParams {
    float saturation;   /* 0.0 = full gray, 1.0 = original (default: 0.0)               */
    Vector3 tintColor;  /* Monochrome tint                  (default: {1, 1, 1})         */
    Vector3 lumaWeights;/* BT.601 luma coefficients         (default: {0.299, 0.587, 0.114}) */
    int loc_saturation;
    int loc_tintColor;
    int loc_lumaWeights;
} ShaderGrayscaleParams;

typedef struct ShaderXBRZParams {
    float sourceWidth;   // Source texture width  (default: 256.0)
    float sourceHeight;  // Source texture height (default: 240.0)
    int loc_resolution;
} ShaderXBRZParams;

typedef struct ShaderNTSCParams {
    float chromaBleed;      // Horizontal color smear   (default: 0.008)
    float noiseAmount;      // Random noise intensity   (default: 0.03)
    float dotCrawlSpeed;    // Chroma anim speed        (default: 1.0)
    float sharpness;        // Luma sharpening boost    (default: 0.5)
    float time;             // Accumulated time — internal
    int loc_chromaBleed;
    int loc_noiseAmount;
    int loc_dotCrawlSpeed;
    int loc_sharpness;
    int loc_time;
    int loc_resolution;
} ShaderNTSCParams;

/**
 * Shader state to house the shader, its type, and the untion for its parameters.
 */
typedef struct LibretroShaderState {
    Shader shader;
    LibretroShaderType type;
    union {
        ShaderCRTParams            crt;
        ShaderScanlinesParams      scanlines;
        ShaderVignetteParams       vignette;
        ShaderGrayscaleParams      grayscale;
        ShaderNTSCParams           ntsc;
        ShaderXBRZParams           xbrz;
    } params;
} LibretroShaderState;

#if defined(__cplusplus)
extern "C" {
#endif

/* Returns the GLSL fragment source for the given shader type.
   Returns "" for LIBRETRO_SHADER_NONE or unsupported GLSL versions.
   The returned pointer is a string literal — do not free it. */
const char* GetLibretroShaderCode(LibretroShaderType type);

/* Returns a LibretroShaderState with default parameters for the given type.
   The shader handle is zeroed — call LoadLibretroShader() to compile it. */
LibretroShaderState GetLibretroShaderDefaults(LibretroShaderType type);

/* Compiles the shader, caches all uniform locations, and uploads initial
   uniform values. Returns a ready-to-use state.
   On failure, state.shader.id == 0 and state.type == LIBRETRO_SHADER_NONE. */
LibretroShaderState LoadLibretroShader(LibretroShaderType type);

/* Same as LoadLibretroShader() but with custom params.
   'params' must point to the correct Shader*Params struct for 'type'. */
LibretroShaderState LoadLibretroShaderEx(LibretroShaderType type, const void *params);

/* Re-uploads all uniforms from state->params, accumulates time for animated
   shaders, and re-uploads resolution on window resize. */
void UpdateLibretroShader(LibretroShaderState *state, float dt);

/* Unloads the GPU shader resource and zeros the handle. */
void UnloadLibretroShader(LibretroShaderState *state);

/* Returns the display name: "CRT", "Scanlines", "None", etc.
   The returned pointer is a string literal — do not free it. */
const char* GetLibretroShaderName(LibretroShaderType type);

/* Load all shaders with their default parameters. */
void LoadLibretroShaders(void);

/* Unload all shader GPU resources. Call before CloseWindow(). */
void UnloadLibretroShaders(void);

/* Update the currently active shader. Also checks F10 (next) and F9
   (previous). Call every frame with GetFrameTime(). */
void UpdateLibretroShaders(float dt);

/* Cycle to the next shader type, wrapping back to LIBRETRO_SHADER_NONE. */
void CycleLibretroShader(void);

/* Cycle to the previous shader type, wrapping from LIBRETRO_SHADER_NONE to last. */
void CycleLibretroShaderReverse(void);

/* Activate a specific shader type. LIBRETRO_SHADER_NONE = pass-through. */
void SetActiveLibretroShader(LibretroShaderType type);
void BeginLibretroShaderGreyscale(void);

/* Returns the currently active shader type. */
LibretroShaderType GetActiveLibretroShaderType(void);

/* Returns a mutable pointer to the active LibretroShaderState,
   or NULL when LIBRETRO_SHADER_NONE is active. */
LibretroShaderState* GetActiveLibretroShaderState(void);

void SetLibretroShaderKeys(KeyboardKey previous, KeyboardKey next);
void BeginLibretroShader(void);
void EndLibretroShader(void);

#if defined(__cplusplus)
}
#endif

#endif /* RAYLIB_LIBRETRO_SHADERS_H */

#ifdef RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION

#if defined(__cplusplus)
extern "C" {
#endif

static LibretroShaderState rlsh_shaders[RAYLIB_LIBRETRO_SHADERS_MAX];
static LibretroShaderType  rlsh_current = LIBRETRO_SHADER_NONE;
static KeyboardKey rlsh_key_prev = KEY_F9;
static KeyboardKey rlsh_key_next = KEY_F10;

const char* GetLibretroShaderCode(LibretroShaderType type) {
    switch (type) {
        case LIBRETRO_SHADER_CRT: return
#if GLSL_VERSION == 330
#include "raylib-libretro-shaders/crt-glsl330.h"
#elif GLSL_VERSION == 120
#include "raylib-libretro-shaders/crt-glsl120.h"
#else
#include "raylib-libretro-shaders/crt-glsl100.h"
#endif
        ;
        case LIBRETRO_SHADER_SCANLINES: return
#if GLSL_VERSION == 330
#include "raylib-libretro-shaders/scanlines-glsl330.h"
#elif GLSL_VERSION == 120
#include "raylib-libretro-shaders/scanlines-glsl120.h"
#else
#include "raylib-libretro-shaders/scanlines-glsl100.h"
#endif
        ;
        case LIBRETRO_SHADER_VIGNETTE: return
#if GLSL_VERSION == 330
#include "raylib-libretro-shaders/vignette-glsl330.h"
#elif GLSL_VERSION == 120
#include "raylib-libretro-shaders/vignette-glsl120.h"
#else
#include "raylib-libretro-shaders/vignette-glsl100.h"
#endif
        ;
        case LIBRETRO_SHADER_GRAYSCALE: return
#if GLSL_VERSION == 330
#include "raylib-libretro-shaders/grayscale-glsl330.h"
#elif GLSL_VERSION == 120
#include "raylib-libretro-shaders/grayscale-glsl120.h"
#else
#include "raylib-libretro-shaders/grayscale-glsl100.h"
#endif
        ;
        case LIBRETRO_SHADER_NTSC: return
#if GLSL_VERSION == 330
#include "raylib-libretro-shaders/ntsc-glsl330.h"
#elif GLSL_VERSION == 120
#include "raylib-libretro-shaders/ntsc-glsl120.h"
#else
#include "raylib-libretro-shaders/ntsc-glsl100.h"
#endif
        ;
        case LIBRETRO_SHADER_XBRZ: return
#if GLSL_VERSION == 330
#include "raylib-libretro-shaders/xbrz-glsl330.h"
#elif GLSL_VERSION == 120
#include "raylib-libretro-shaders/xbrz-glsl120.h"
#else
#include "raylib-libretro-shaders/xbrz-glsl100.h"
#endif
        ;
        default: return "";
    }
}

/* ----------------------------------------------------------------
 * GetShaderDefaults
 * ---------------------------------------------------------------- */

LibretroShaderState GetLibretroShaderDefaults(LibretroShaderType type) {
    LibretroShaderState state = { 0 };
    state.type = type;
    switch (type) {
        case LIBRETRO_SHADER_CRT:
            state.params.crt.brightness        = 1.2f;
            state.params.crt.scanlineIntensity = 2.0f;
            state.params.crt.curvatureRadius   = 0.4f;
            state.params.crt.cornerSize        = 5.0f;
            state.params.crt.cornerSmooth      = 35.0f;
            state.params.crt.curvature         = 1;
            state.params.crt.border            = 1;
            break;
        case LIBRETRO_SHADER_SCANLINES:
            state.params.scanlines.frequency = 150.0f;
            state.params.scanlines.opacity   = 0.5f;
            state.params.scanlines.offset    = 0.0f;
            state.params.scanlines.time      = 0.0f;
            break;
        case LIBRETRO_SHADER_VIGNETTE:
            state.params.vignette.radius    = 0.75f;
            state.params.vignette.softness  = 0.45f;
            state.params.vignette.opacity   = 0.6f;
            state.params.vignette.tintColor = (Vector3){ 0.0f, 0.0f, 0.0f };
            break;
        case LIBRETRO_SHADER_GRAYSCALE:
            state.params.grayscale.saturation   = 0.0f;
            state.params.grayscale.tintColor    = (Vector3){ 1.0f, 1.0f, 1.0f };
            state.params.grayscale.lumaWeights  = (Vector3){ 0.299f, 0.587f, 0.114f };
            break;
        case LIBRETRO_SHADER_NTSC:
            state.params.ntsc.chromaBleed   = 0.008f;
            state.params.ntsc.noiseAmount   = 0.03f;
            state.params.ntsc.dotCrawlSpeed = 1.0f;
            state.params.ntsc.sharpness     = 0.5f;
            state.params.ntsc.time          = 0.0f;
            break;
        case LIBRETRO_SHADER_XBRZ:
            state.params.xbrz.sourceWidth  = 256.0f;
            state.params.xbrz.sourceHeight = 240.0f;
            break;
        default: break;
    }
    return state;
}

/* ----------------------------------------------------------------
 * Internal: upload resolution vec2 to a cached location
 * ---------------------------------------------------------------- */

static void rlsh_set_resolution(Shader shader, int loc) {
    if (loc >= 0) {
        Vector2 res = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        SetShaderValue(shader, loc, &res, SHADER_UNIFORM_VEC2);
    }
}

/* ----------------------------------------------------------------
 * LoadLibretroShaderEx
 * ---------------------------------------------------------------- */

LibretroShaderState LoadLibretroShaderEx(LibretroShaderType type, const void *params) {
    LibretroShaderState state = { 0 };
    state.type = type;

    const char *code = GetLibretroShaderCode(type);
    if (!code || code[0] == '\0') return state;

    state.shader = LoadShaderFromMemory(0, code);

    switch (type) {
        case LIBRETRO_SHADER_CRT: {
            ShaderCRTParams *p = &state.params.crt;
            if (params) *p = *(const ShaderCRTParams *)params;
            p->loc_resolution        = GetShaderLocation(state.shader, "resolution");
            p->loc_brightness        = GetShaderLocation(state.shader, "Brightness");
            p->loc_scanlineIntensity = GetShaderLocation(state.shader, "ScanlineIntensity");
            p->loc_curvatureRadius   = GetShaderLocation(state.shader, "CurvatureRadius");
            p->loc_cornerSize        = GetShaderLocation(state.shader, "CornerSize");
            p->loc_cornerSmooth      = GetShaderLocation(state.shader, "Cornersmooth");
            p->loc_curvature         = GetShaderLocation(state.shader, "Curvature");
            p->loc_border            = GetShaderLocation(state.shader, "Border");
            rlsh_set_resolution(state.shader, p->loc_resolution);
            SetShaderValue(state.shader, p->loc_brightness,        &p->brightness,        SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_scanlineIntensity, &p->scanlineIntensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_curvatureRadius,   &p->curvatureRadius,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_cornerSize,        &p->cornerSize,        SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_cornerSmooth,      &p->cornerSmooth,      SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_curvature,         &p->curvature,         SHADER_UNIFORM_INT);
            SetShaderValue(state.shader, p->loc_border,            &p->border,            SHADER_UNIFORM_INT);
        } break;

        case LIBRETRO_SHADER_SCANLINES: {
            ShaderScanlinesParams *p = &state.params.scanlines;
            if (params) *p = *(const ShaderScanlinesParams *)params;
            p->loc_frequency = GetShaderLocation(state.shader, "frequency");
            p->loc_opacity   = GetShaderLocation(state.shader, "opacity");
            p->loc_offset    = GetShaderLocation(state.shader, "offset");
            p->loc_time      = GetShaderLocation(state.shader, "time");
            SetShaderValue(state.shader, p->loc_frequency, &p->frequency, SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_opacity,   &p->opacity,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_offset,    &p->offset,    SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_time,      &p->time,      SHADER_UNIFORM_FLOAT);
        } break;

        case LIBRETRO_SHADER_VIGNETTE: {
            ShaderVignetteParams *p = &state.params.vignette;
            if (params) *p = *(const ShaderVignetteParams *)params;
            p->loc_radius    = GetShaderLocation(state.shader, "radius");
            p->loc_softness  = GetShaderLocation(state.shader, "softness");
            p->loc_opacity   = GetShaderLocation(state.shader, "opacity");
            p->loc_tintColor = GetShaderLocation(state.shader, "tintColor");
            SetShaderValue(state.shader, p->loc_radius,    &p->radius,    SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_softness,  &p->softness,  SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_opacity,   &p->opacity,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_tintColor, &p->tintColor, SHADER_UNIFORM_VEC3);
        } break;

        case LIBRETRO_SHADER_GRAYSCALE: {
            ShaderGrayscaleParams *p = &state.params.grayscale;
            if (params) *p = *(const ShaderGrayscaleParams *)params;
            p->loc_saturation   = GetShaderLocation(state.shader, "saturation");
            p->loc_tintColor    = GetShaderLocation(state.shader, "tintColor");
            p->loc_lumaWeights  = GetShaderLocation(state.shader, "lumaWeights");
            SetShaderValue(state.shader, p->loc_saturation,  &p->saturation,  SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_tintColor,   &p->tintColor,   SHADER_UNIFORM_VEC3);
            SetShaderValue(state.shader, p->loc_lumaWeights, &p->lumaWeights, SHADER_UNIFORM_VEC3);
        } break;

        case LIBRETRO_SHADER_NTSC: {
            ShaderNTSCParams *p = &state.params.ntsc;
            if (params) *p = *(const ShaderNTSCParams *)params;
            p->loc_chromaBleed   = GetShaderLocation(state.shader, "chromaBleed");
            p->loc_noiseAmount   = GetShaderLocation(state.shader, "noiseAmount");
            p->loc_dotCrawlSpeed = GetShaderLocation(state.shader, "dotCrawlSpeed");
            p->loc_sharpness     = GetShaderLocation(state.shader, "sharpness");
            p->loc_time          = GetShaderLocation(state.shader, "time");
            p->loc_resolution    = GetShaderLocation(state.shader, "resolution");
            SetShaderValue(state.shader, p->loc_chromaBleed,   &p->chromaBleed,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_noiseAmount,   &p->noiseAmount,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_dotCrawlSpeed, &p->dotCrawlSpeed, SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_sharpness,     &p->sharpness,     SHADER_UNIFORM_FLOAT);
            SetShaderValue(state.shader, p->loc_time,          &p->time,          SHADER_UNIFORM_FLOAT);
            rlsh_set_resolution(state.shader, p->loc_resolution);
        } break;

        case LIBRETRO_SHADER_XBRZ: {
            ShaderXBRZParams *p = &state.params.xbrz;
            if (params) *p = *(const ShaderXBRZParams *)params;
            p->loc_resolution = GetShaderLocation(state.shader, "resolution");
            Vector2 res = { p->sourceWidth, p->sourceHeight };
            SetShaderValue(state.shader, p->loc_resolution, &res, SHADER_UNIFORM_VEC2);
        } break;

        default: break;
    }
    return state;
}

LibretroShaderState LoadLibretroShader(LibretroShaderType type) {
    LibretroShaderState defaults = GetLibretroShaderDefaults(type);
    return LoadLibretroShaderEx(type, &defaults.params);
}

/* ----------------------------------------------------------------
 * UpdateLibretroShader
 * ---------------------------------------------------------------- */

void UpdateLibretroShader(LibretroShaderState *state, float dt) {
    if (!state || state->type == LIBRETRO_SHADER_NONE || state->shader.id == 0) return;

    int resized = IsWindowResized();

    switch (state->type) {
        case LIBRETRO_SHADER_CRT: {
            ShaderCRTParams *p = &state->params.crt;
            if (resized) rlsh_set_resolution(state->shader, p->loc_resolution);
            SetShaderValue(state->shader, p->loc_brightness,        &p->brightness,        SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_scanlineIntensity, &p->scanlineIntensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_curvatureRadius,   &p->curvatureRadius,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_cornerSize,        &p->cornerSize,        SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_cornerSmooth,      &p->cornerSmooth,      SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_curvature,         &p->curvature,         SHADER_UNIFORM_INT);
            SetShaderValue(state->shader, p->loc_border,            &p->border,            SHADER_UNIFORM_INT);
        } break;

        case LIBRETRO_SHADER_SCANLINES: {
            ShaderScanlinesParams *p = &state->params.scanlines;
            p->time += dt;
            SetShaderValue(state->shader, p->loc_frequency, &p->frequency, SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_opacity,   &p->opacity,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_offset,    &p->offset,    SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_time,      &p->time,      SHADER_UNIFORM_FLOAT);
        } break;

        case LIBRETRO_SHADER_VIGNETTE: {
            ShaderVignetteParams *p = &state->params.vignette;
            SetShaderValue(state->shader, p->loc_radius,    &p->radius,    SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_softness,  &p->softness,  SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_opacity,   &p->opacity,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_tintColor, &p->tintColor, SHADER_UNIFORM_VEC3);
        } break;

        case LIBRETRO_SHADER_GRAYSCALE: {
            ShaderGrayscaleParams *p = &state->params.grayscale;
            SetShaderValue(state->shader, p->loc_saturation,  &p->saturation,  SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_tintColor,   &p->tintColor,   SHADER_UNIFORM_VEC3);
            SetShaderValue(state->shader, p->loc_lumaWeights, &p->lumaWeights, SHADER_UNIFORM_VEC3);
        } break;

        case LIBRETRO_SHADER_NTSC: {
            ShaderNTSCParams *p = &state->params.ntsc;
            p->time += dt;
            if (resized) rlsh_set_resolution(state->shader, p->loc_resolution);
            SetShaderValue(state->shader, p->loc_chromaBleed,   &p->chromaBleed,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_noiseAmount,   &p->noiseAmount,   SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_dotCrawlSpeed, &p->dotCrawlSpeed, SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_sharpness,     &p->sharpness,     SHADER_UNIFORM_FLOAT);
            SetShaderValue(state->shader, p->loc_time,          &p->time,          SHADER_UNIFORM_FLOAT);
        } break;

        case LIBRETRO_SHADER_XBRZ: {
            ShaderXBRZParams *p = &state->params.xbrz;
            // xBRZ needs the proper texture width/height.
            Vector2 res = { (float)GetLibretroWidth(), (float)GetLibretroHeight() };
            SetShaderValue(state->shader, p->loc_resolution, &res, SHADER_UNIFORM_VEC2);
        } break;

        default: break;
    }
}

/* ----------------------------------------------------------------
 * UnloadLibretroShader
 * ---------------------------------------------------------------- */

void UnloadLibretroShader(LibretroShaderState *state) {
    if (!state) return;
    UnloadShader(state->shader);
    state->shader.id = 0;
    state->type = LIBRETRO_SHADER_NONE;
}

/* ----------------------------------------------------------------
 * GetShaderName
 * ---------------------------------------------------------------- */

const char* GetLibretroShaderName(LibretroShaderType type) {
    switch (type) {
        case LIBRETRO_SHADER_NONE:            return "None";
        case LIBRETRO_SHADER_CRT:             return "CRT";
        case LIBRETRO_SHADER_SCANLINES:       return "Scanlines";
        case LIBRETRO_SHADER_VIGNETTE:        return "Vignette";
        case LIBRETRO_SHADER_GRAYSCALE:       return "Grayscale";
        case LIBRETRO_SHADER_NTSC:            return "NTSC";
        case LIBRETRO_SHADER_XBRZ:            return "xBRZ";
        default:                              return "Unknown";
    }
}

/* ----------------------------------------------------------------
 * Global shader management
 * ---------------------------------------------------------------- */

void LoadLibretroShaders(void) {
    for (int i = 0; i < RAYLIB_LIBRETRO_SHADERS_MAX; i++) {
        rlsh_shaders[i] = LoadLibretroShader((LibretroShaderType)(i + 1));
    }
}

void UnloadLibretroShaders(void) {
    for (int i = 0; i < RAYLIB_LIBRETRO_SHADERS_MAX; i++) {
        UnloadLibretroShader(&rlsh_shaders[i]);
    }
}

void UpdateLibretroShaders(float dt) {
    if (rlsh_key_prev != KEY_NULL && IsKeyReleased(rlsh_key_prev)) CycleLibretroShaderReverse();
    if (rlsh_key_next != KEY_NULL && IsKeyReleased(rlsh_key_next)) CycleLibretroShader();
    if (rlsh_current != LIBRETRO_SHADER_NONE) {
        UpdateLibretroShader(&rlsh_shaders[(int)rlsh_current - 1], dt);
    }
}

void SetLibretroShaderKeys(KeyboardKey previous, KeyboardKey next) {
    rlsh_key_prev = previous;
    rlsh_key_next = next;
}

void CycleLibretroShader(void) {
    int next = (int)rlsh_current + 1;
    if (next >= LIBRETRO_SHADER_TYPE_COUNT) next = LIBRETRO_SHADER_NONE;
    SetActiveLibretroShader((LibretroShaderType)next);
}

void CycleLibretroShaderReverse(void) {
    int prev = (int)rlsh_current - 1;
    if (prev < LIBRETRO_SHADER_NONE) prev = LIBRETRO_SHADER_TYPE_COUNT - 1;
    SetActiveLibretroShader((LibretroShaderType)prev);
}

void SetActiveLibretroShader(LibretroShaderType type) {
    rlsh_current = type;
}

LibretroShaderType GetActiveLibretroShaderType(void) {
    return rlsh_current;
}

LibretroShaderState* GetActiveLibretroShaderState(void) {
    if (rlsh_current == LIBRETRO_SHADER_NONE) return 0;
    return &rlsh_shaders[(int)rlsh_current - 1];
}

void BeginLibretroShader(void) {
    if (rlsh_current != LIBRETRO_SHADER_NONE)
        BeginShaderMode(rlsh_shaders[(int)rlsh_current - 1].shader);
}

void EndLibretroShader(void) {
    if (rlsh_current != LIBRETRO_SHADER_NONE)
        EndShaderMode();
}

void BeginLibretroShaderGreyscale(void) {
    BeginShaderMode(rlsh_shaders[LIBRETRO_SHADER_GRAYSCALE-1].shader);
}

#if defined(__cplusplus)
}
#endif

#endif /* RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION */
