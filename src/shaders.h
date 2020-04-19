#ifndef RAYLIB_LIBRETRO_SHADERS
#define RAYLIB_LIBRETRO_SHADERS
#include "raylib.h"
#include "../vendor/incbin/incbin.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#if GLSL_VERSION == 330
INCBIN(ShaderCrt, "../src/shaders/crt/resources/shaders/glsl330/crt.fs");
INCBIN(ShaderScanlines, "../vendor/raylib/examples/shaders/resources/shaders/glsl330/scanlines.fs");
#elif GLSL_VERSION == 100
INCBIN(ShaderCrt, "../src/shaders/crt/resources/shaders/glsl100/crt.fs");
INCBIN(ShaderScanlines, "../vendor/raylib/examples/shaders/resources/shaders/glsl100/scanlines.fs");
#elif GLSL_VERSION == 120
const unsigned char gShaderCrtData[0];
INCBIN(ShaderScanlines, "../vendor/raylib/examples/shaders/resources/shaders/glsl120/scanlines.fs");
#else
const unsigned char gShaderCrtData[0];
const unsigned char gShaderScanlinesData[0];
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


Shader LoadShaderCRT() {
    const char* code = TextFormat("%s", gShaderCrtData);
    //TraceLog(LOG_INFO, code);
    Shader shader = LoadShaderCode(NULL, code);
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
    // Check to see if we are to change the shader.
    if (IsKeyPressed(KEY_F10)) {
        if (++currentShader > MAX_SHADERS) {
            currentShader = 0;
        }
    }

    // Update resolution of CRT shader.
    if (IsWindowResized()) {
        SetShaderValue(shaders[0], GetShaderLocation(shaders[0], "resolution"), &((Vector2){GetScreenWidth(), GetScreenHeight()}), 1);
    }
}

void LoadShaders() {
    shaders[0] = LoadShaderCRT();
    shaders[1] = LoadShaderCode(NULL, gShaderScanlinesData);
}

void UnloadShaders() {
    for (int i = 0; i < MAX_SHADERS; i++) {
        UnloadShader(shaders[i]);
    }
}

#endif // RAYLIB_LIBRETRO_SHADERS
