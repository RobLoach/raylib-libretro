#ifndef RAYLIB_LIBRETRO_SHADERS
#define RAYLIB_LIBRETRO_SHADERS
#include "raylib-cpp.hpp"

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

class Shaders {
    public:
    std::vector<raylib::Shader> shaders;
    int currentShader = 0;
    ShaderCRT shaderCRT;

    Shaders() {
        shaders.emplace_back(LoadShaderCRT());
        shaders.emplace_back(LoadShaderScanlines());
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
        Vector2 screenSize = {GetScreenWidth(), GetScreenHeight()};
        SetShaderValue(shader, GetShaderLocation(shader, "resolution"), &screenSize, UNIFORM_VEC2);
        SetShaderValue(shader, brightnessLoc, &shaderCRT.brightness, UNIFORM_FLOAT);
        SetShaderValue(shader, ScanlineIntensityLoc, &shaderCRT.scanlineIntensity, UNIFORM_FLOAT);
        SetShaderValue(shader, curvatureRadiusLoc, &shaderCRT.curvatureRadius, UNIFORM_FLOAT);
        SetShaderValue(shader, cornerSizeLoc, &shaderCRT.cornerSize, UNIFORM_FLOAT);
        SetShaderValue(shader, cornersmoothLoc, &shaderCRT.cornersmooth, UNIFORM_FLOAT);
        SetShaderValue(shader, curvatureLoc, &shaderCRT.curvature, UNIFORM_FLOAT);
        SetShaderValue(shader, borderLoc, &shaderCRT.border, UNIFORM_FLOAT);
        return shader;
    }

    Shader LoadShaderScanlines() {
        return LoadShaderCode(NULL, GetShaderCodeScanLines());
    }

    void Update() {
        // Check to see if we are to change the shader.
        if (IsKeyPressed(KEY_F10)) {
            if (++currentShader > shaders.size()) {
                currentShader = 0;
            }
        }

        // Update resolution of CRT shader.
        if (IsWindowResized()) {
            Vector2 screenSize = {GetScreenWidth(), GetScreenHeight()};
            SetShaderValue(shaders[0], GetShaderLocation(shaders[0], "resolution"), &screenSize, 1);
        }
    }

    void Begin() {
        if (currentShader > 0) {
            shaders[currentShader - 1].BeginMode();
        }
    }
    void End() {
        if (currentShader > 0) {
            EndShaderMode();
        }
    }
};

#endif // RAYLIB_LIBRETRO_SHADERS
