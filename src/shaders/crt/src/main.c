/*******************************************************************************************
*
*   raylib [shaders] example - Texture Waves
*
*   NOTE: This example requires raylib OpenGL 3.3 or ES2 versions for shaders support,
*         OpenGL 1.1 does not support shaders, recompile raylib to OpenGL 3.3 version.
*
*   NOTE: Shaders used in this example are #version 330 (OpenGL 3.3), to test this example
*         on OpenGL ES 2.0 platforms (Android, Raspberry Pi, HTML5), use #version 100 shaders
*         raylib comes with shaders ready for both versions, check raylib/shaders install folder
*
*   This example has been created using raylib 2.6 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Example contributed by Anata (@anatagawa)
*
*   Copyright (c) 2019 Anata (@anatagawa) and Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            300
#else   // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define max(a, b) ((a)>(b)? (a) : (b))
#define min(a, b) ((a)<(b)? (a) : (b))

// Shaders Uniforms
typedef struct ShaderCRT {
	float brightness;
	float scanlineIntensity;
	float curvatureRadius;
	float cornerSize;
	float cornersmooth;
	float curvature;
	float border;
} ShaderCRT;

int main(void)
{
	// Initialization
	//--------------------------------------------------------------------------------------
	const int screenWidth = 640;
	const int screenHeight = 480;

	Vector2 gameScreen = {640,480};
	Vector2 oldGameScreen = {640,480};

	SetConfigFlags( FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE );

	InitWindow(screenWidth, screenHeight, "raylib [shaders] example - crt");
	SetWindowMinSize( gameScreen.x, gameScreen.y );

	// Load texture texture to apply shaders
	RenderTexture2D target = LoadRenderTexture( gameScreen.x, gameScreen.y );
	Texture2D texture = LoadTexture("resources/space.png");

	// Load shader and setup location points and values
	Shader shader = LoadShader(0, FormatText("resources/shaders/glsl%i/crt.fs", GLSL_VERSION));

	float brightnessLoc = GetShaderLocation(shader, "Brightness");
	float ScanlineIntensityLoc = GetShaderLocation(shader, "ScanlineIntensity");
	float curvatureRadiusLoc = GetShaderLocation(shader, "CurvatureRadius");
	float cornerSizeLoc = GetShaderLocation(shader, "CornerSize");
	float cornersmoothLoc = GetShaderLocation(shader, "Cornersmooth");
	float curvatureLoc = GetShaderLocation(shader, "Curvature");
	float borderLoc = GetShaderLocation(shader, "Border");

	ShaderCRT shaderCRT = {
		1.0f,	// brightness
		0.5f,	// scanlineIntensity
		0.4f,	// curvatureRadius
		5.0f,	// cornerSize
		35.0f,	// cornersmooth
		true,	// curvature
		true	// border
	};

	SetShaderValue(shader, GetShaderLocation(shader, "resolution"), &((Vector2){GetScreenWidth(), GetScreenHeight()}), UNIFORM_VEC2);
	SetShaderValue(shader, brightnessLoc, &shaderCRT.brightness, UNIFORM_FLOAT);
	SetShaderValue(shader, ScanlineIntensityLoc, &shaderCRT.scanlineIntensity, UNIFORM_FLOAT);
	SetShaderValue(shader, curvatureRadiusLoc, &shaderCRT.curvatureRadius, UNIFORM_FLOAT);
	SetShaderValue(shader, cornerSizeLoc, &shaderCRT.cornerSize, UNIFORM_FLOAT);
	SetShaderValue(shader, cornersmoothLoc, &shaderCRT.cornersmooth, UNIFORM_FLOAT);
	SetShaderValue(shader, curvatureLoc, &shaderCRT.curvature, UNIFORM_FLOAT);
	SetShaderValue(shader, borderLoc, &shaderCRT.border, UNIFORM_FLOAT);

	int background_X = 0;

	SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
	// -------------------------------------------------------------------------------------------------------------

	// Main game loop
	while (!WindowShouldClose())    // Detect window close button or ESC key
	{
		// Update
		//----------------------------------------------------------------------------------

		// Compute required framebuffer scaling
        float scale = min((float)GetScreenWidth()/gameScreen.x, (float)GetScreenHeight()/gameScreen.y);

		// Check resolution change
		if(oldGameScreen.x !=GetScreenWidth() || oldGameScreen.y != GetScreenHeight()) {
			SetShaderValue(shader, GetShaderLocation(shader, "resolution"), &((Vector2){GetScreenWidth(), GetScreenHeight()}), UNIFORM_VEC2);
			oldGameScreen = (Vector2){ GetScreenWidth(), GetScreenHeight()};
		}


		// Move background and reset position at -640
		background_X -= 4;
		background_X %= 640;

		// Draw
		//----------------------------------------------------------------------------------
		BeginDrawing();

			ClearBackground(BLACK);

			// Draw everything in the render texture, note this will not be rendered on screen, yet
			BeginTextureMode(target);
				ClearBackground(BLANK);         // Clear render texture background color

				DrawTextureQuad( texture, (Vector2){2,1}, (Vector2){0}, (Rectangle){ background_X, 0, texture.width * 2, texture.height }, WHITE  );
			EndTextureMode();

			BeginShaderMode(shader);
			
				// Draw RenderTexture2D to window, properly scaled
				DrawTexturePro(target.texture, (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
							(Rectangle){ (GetScreenWidth() - ((float)gameScreen.x*scale))*0.5, (GetScreenHeight() - ((float)gameScreen.y*scale))*0.5,
							(float)gameScreen.x*scale, (float)gameScreen.y*scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
				
			EndShaderMode();

		EndDrawing();
		//----------------------------------------------------------------------------------
	}

	// De-Initialization
	//--------------------------------------------------------------------------------------
	UnloadShader(shader);         	// Unload shader
	UnloadTexture(texture);       	// Unload texture
	UnloadRenderTexture(target);	// Unload render texture

	CloseWindow();              // Close window and OpenGL context
	//--------------------------------------------------------------------------------------

	return 0;
}
