#ifndef RAYLIB_LIBRETRO_SHADERS
#define RAYLIB_LIBRETRO_SHADERS
#include "raylib.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

typedef struct ShaderCRT {
    float brightness;
    float scanlineIntensity;
    float curvatureRadius;
    float cornerSize;
    float cornersmooth;
    float curvature;
    float border;
} ShaderCRT;

#define MAX_SHADERS 2
Shader shaders[MAX_SHADERS];
ShaderCRT shaderCRT;
int currentShader = 0;

/**
 * Loads the scanline shader code at compile time.
 */
const char* GetShaderCodeScanLines() {
    const char* code =
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
    return code;
}

/**
 * Loads the CRT shader code at compile time.
 */
const char* GetShaderCodeCRT() {
    const char* code =
#if GLSL_VERSION == 100
#include "shaders/crt-glsl100.txt"
#elif GLSL_VERSION == 330
#include "shaders/crt-glsl330.txt"
#else
    ""
#endif
;
    return code;
}

Shader LoadShaderCRT() {
    Shader shader = LoadShaderCode(NULL, GetShaderCodeCRT());
    float brightnessLoc = GetShaderLocation(shader, "Brightness");
    float ScanlineIntensityLoc = GetShaderLocation(shader, "ScanlineIntensity");
    float curvatureRadiusLoc = GetShaderLocation(shader, "CurvatureRadius");
    float cornerSizeLoc = GetShaderLocation(shader, "CornerSize");
    float cornersmoothLoc = GetShaderLocation(shader, "Cornersmooth");
    float curvatureLoc = GetShaderLocation(shader, "Curvature");
    float borderLoc = GetShaderLocation(shader, "Border");
    shaderCRT.brightness = 1.0f;
    shaderCRT.scanlineIntensity = 0.5f;
    shaderCRT.curvatureRadius = 0.4f;
    shaderCRT.cornerSize = 5.0f;
    shaderCRT.cornersmooth = 35.0f;
    shaderCRT.curvature = true;
    shaderCRT.border = true;
    SetShaderValue(shader, GetShaderLocation(shader, "resolution"), &((Vector2){GetScreenWidth(), GetScreenHeight()}), UNIFORM_VEC2);
    SetShaderValue(shader, brightnessLoc, &shaderCRT.brightness, UNIFORM_FLOAT);
    SetShaderValue(shader, ScanlineIntensityLoc, &shaderCRT.scanlineIntensity, UNIFORM_FLOAT);
    SetShaderValue(shader, curvatureRadiusLoc, &shaderCRT.curvatureRadius, UNIFORM_FLOAT);
    SetShaderValue(shader, cornerSizeLoc, &shaderCRT.cornerSize, UNIFORM_FLOAT);
    SetShaderValue(shader, cornersmoothLoc, &shaderCRT.cornersmooth, UNIFORM_FLOAT);
    SetShaderValue(shader, curvatureLoc, &shaderCRT.curvature, UNIFORM_FLOAT);
    SetShaderValue(shader, borderLoc, &shaderCRT.border, UNIFORM_FLOAT);
    return shader;
}

void UpdateShaders() {
    if (IsKeyPressed(KEY_F10)) {
        if (++currentShader > MAX_SHADERS) {
            currentShader = 0;
        }
    }
}

void LoadShaders() {
    shaders[0] = LoadShaderCRT();
    shaders[1] = LoadShaderCode(NULL, GetShaderCodeScanLines());
}

void UnloadShaders() {
    for (int i = 0; i < MAX_SHADERS; i++) {
        UnloadShader(shaders[i]);
    }
}

#endif // RAYLIB_LIBRETRO_SHADERS
