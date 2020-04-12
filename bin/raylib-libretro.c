/*******************************************************************************************
*
*   raylib-libretro v0.0.1 - A libretro frontend for raylib.
*
*   DESCRIPTION:
*
*   raylib-libretro [core] <game>
*
*   CONTRIBUTORS:
*       Rob Loach:          Initial implementation
*
*   LICENSE: GPL-3.0
*
**********************************************************************************************/

#include "raylib.h"
#include "../include/rLibretro.h"

int main(int argc, char* argv[]) {
	// Ensure proper amount of arguments.
	if (argc <= 1) {
		TraceLog(LOG_ERROR, "Usage: %s <core> [game]", argv[0]);
		return 1;
	}

	// Create the window.
	InitWindow(800, 600, "raylib-libretro");

	// Load the given core.
	if (!LibretroLoadCore(argv[1], false)) {
		TraceLog(LOG_FATAL, "Failed to load given core: %s", argv[1]);
		CloseWindow();
		return 1;
	}

	// Load the given game.
	const char* gameFile = (argc > 2) ? argv[2] : NULL;
	if (!LibretroLoadGame(gameFile)) {
		TraceLog(LOG_FATAL, "Failed to load game. %s", gameFile);
		CloseWindow();
		return 1;
	}

	// Initialize the systems.
	LibretroInit();

	while (!WindowShouldClose())
	{
		// Run a frame of the core.
		LibretroUpdate();

		// See if we should close the game.
		if (LibretroShouldClose()) {
			break;
		}

		// Render the libretro core.
		BeginDrawing();
		{
			ClearBackground(BLACK);
			LibretroDraw();
		}
		EndDrawing();
	}

	// Unload the game and close the core.
	LibretroUnloadGame();
	LibretroClose();

	CloseWindow();

	return 0;
}