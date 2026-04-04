#ifndef RAYLIB_LIBRETRO_SHADERS_H
#define RAYLIB_LIBRETRO_SHADERS_H

#include "raylib.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION 330
#else
    #define GLSL_VERSION 100
#endif

#define RAYLIB_LIBRETRO_SHADERS_MAX 2

const char* GetShaderCodeScanLines(void);
const char* GetShaderCodeCRT(void);
Shader LoadShaderCRT(void);
void LoadShaders(void);
void UnloadShaders(void);
void UpdateShaders(void);
void BeginLibretroShader(void);
void EndLibretroShader(void);
void DrawShadersGui(Rectangle rect);

#endif // RAYLIB_LIBRETRO_SHADERS_H

#ifdef RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION

Shader raylib_libretro_shaders[RAYLIB_LIBRETRO_SHADERS_MAX];
int raylib_libretro_shader_current = 0;

const char* GetShaderCodeScanLines(void) {
    return
#if GLSL_VERSION == 100
#include "shaders/scanlines-glsl100.txt"
#elif GLSL_VERSION == 120
#include "shaders/scanlines-glsl120.txt"
#elif GLSL_VERSION == 330
#include "shaders/scanlines-glsl330.txt"
#else
    ""
#endif
    ;
}

const char* GetShaderCodeCRT(void) {
    return
#if GLSL_VERSION == 100
#include "shaders/crt-glsl100.txt"
#elif GLSL_VERSION == 120
#include "shaders/crt-glsl120.txt"
#elif GLSL_VERSION == 330
#include "shaders/crt-glsl330.txt"
#else
    ""
#endif
    ;
}

Shader LoadShaderCRT(void) {
    Shader shader = LoadShaderFromMemory(NULL, GetShaderCodeCRT());
    float brightness        = 1.0f;
    float scanlineIntensity = 0.5f;
    float curvatureRadius   = 0.4f;
    float cornerSize        = 5.0f;
    float cornersmooth      = 35.0f;
    float curvature         = 1.0f;
    float border            = 1.0f;
    SetShaderValue(shader, GetShaderLocation(shader, "resolution"),        &((Vector2){GetScreenWidth(), GetScreenHeight()}), SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, GetShaderLocation(shader, "Brightness"),        &brightness,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "ScanlineIntensity"), &scanlineIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "CurvatureRadius"),   &curvatureRadius,   SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "CornerSize"),        &cornerSize,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "Cornersmooth"),      &cornersmooth,      SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "Curvature"),         &curvature,         SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "Border"),            &border,            SHADER_UNIFORM_FLOAT);
    return shader;
}

void LoadShaders(void) {
    raylib_libretro_shaders[0] = LoadShaderCRT();
    raylib_libretro_shaders[1] = LoadShaderFromMemory(NULL, GetShaderCodeScanLines());
}

void UnloadShaders(void) {
    for (int i = 0; i < RAYLIB_LIBRETRO_SHADERS_MAX; i++) {
        UnloadShader(raylib_libretro_shaders[i]);
    }
}

void UpdateShaders(void) {
    if (IsKeyPressed(KEY_F10)) {
        if (++raylib_libretro_shader_current > RAYLIB_LIBRETRO_SHADERS_MAX) {
            raylib_libretro_shader_current = 0;
        }
    }

    if (IsWindowResized()) {
        SetShaderValue(raylib_libretro_shaders[0], GetShaderLocation(raylib_libretro_shaders[0], "resolution"), &((Vector2){GetScreenWidth(), GetScreenHeight()}), SHADER_UNIFORM_VEC2);
    }
}

void BeginLibretroShader(void) {
    if (raylib_libretro_shader_current > 0) {
        BeginShaderMode(raylib_libretro_shaders[raylib_libretro_shader_current - 1]);
    }
}

void EndLibretroShader(void) {
    if (raylib_libretro_shader_current > 0) {
        EndShaderMode();
    }
}

void DrawShadersGui(Rectangle rect) {
    GuiToggleGroup(rect, "Pixel Perfect;CRT;Scanlines", &raylib_libretro_shader_current);
}

#endif // RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
