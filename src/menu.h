#ifndef RAYLIB_LIBRETRO_MENU
#define RAYLIB_LIBRETRO_MENU
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include "raygui.h"

#define GUI_FILE_DIALOG_IMPLEMENTATION
#include "../examples/custom_file_dialog/gui_file_dialog.h"

#include "../include/rlibretro.h"
#include "shaders.h"

bool menuActive = true;
GuiFileDialogState openFileDialog;
int openFileType = 0;
char* menuMessage;

void InitMenu() {
    // TODO: Apply a raygui style? The following is Ashes.
    // GuiSetStyle(0, 0, 0xf0f0f0ff);
    // GuiSetStyle(0, 1, 0x868686ff);
    // GuiSetStyle(0, 2, 0xe6e6e6ff);
    // GuiSetStyle(0, 3, 0x929999ff);
    // GuiSetStyle(0, 4, 0xeaeaeaff);
    // GuiSetStyle(0, 5, 0x98a1a8ff);
    // GuiSetStyle(0, 6, 0x3f3f3fff);
    // GuiSetStyle(0, 7, 0xf6f6f6ff);
    // GuiSetStyle(0, 8, 0x414141ff);
    // GuiSetStyle(0, 9, 0x8b8b8bff);
    // GuiSetStyle(0, 10, 0x777777ff);
    // GuiSetStyle(0, 11, 0x959595ff);
    // GuiSetStyle(0, 16, 0x00000010);
    // GuiSetStyle(0, 17, 0x00000001);
    // GuiSetStyle(0, 18, 0x9dadb1ff);
    // GuiSetStyle(0, 19, 0x6b6b6bff);

    GuiFade(0.9f);
    openFileDialog = InitGuiFileDialog(GetScreenWidth() * 0.8f, GetScreenHeight() * 0.8f, GetWorkingDirectory(), false);
    TextCopy(openFileDialog.dirPathText, GetWorkingDirectory());
}

void SetMenuActive(bool enabled) {
    menuActive = enabled;
}

void UpdateMenu() {
    // Toggle the menu.
    if (IsKeyReleased(KEY_F1) || IsGamepadButtonReleased(0, GAMEPAD_BUTTON_MIDDLE)) {
        menuActive = !menuActive;
    }
    if (!menuActive) {
        // If we request to close the menu, and game isn't loaded, force the menu open.
        if (!IsLibretroGameReady()) {
            menuActive = true;
        }
        else {
            // Don't display the menu.
            return;
        }
    }

    // Default sizes.
    int buttonWidth = 160;
    int buttonHeight = 50;
    int padding = 50;
    int smallPadding = 25;

    // Background
    GuiPanel((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});

    if (!openFileDialog.fileDialogActive) {
        // On the main menu, have a large font.
        GuiSetStyle(DEFAULT, TEXT_SIZE, 20);

        // Close Menu
        GuiSetTooltip("Close Menu");
        Rectangle closeButton = (Rectangle){GetScreenWidth() - buttonHeight - padding, padding, buttonHeight, buttonHeight};
        if (GuiButton(closeButton, GuiIconText(RICON_CROSS, ""))) {
            if (IsLibretroGameReady()) {
                menuActive = false;
                return;
            }
        }
        GuiClearTooltip();

        // Fullscreen
        GuiSetTooltip("Toggle Fullscreen");
        Rectangle fullscreenCheckbox = (Rectangle){GetScreenWidth() - buttonHeight * 2 - padding - smallPadding, padding, buttonHeight, buttonHeight};
        if (GuiButton(fullscreenCheckbox, GuiIconText(RICON_ZOOM_CENTER, ""))) {
            ToggleFullscreen();
        }
        GuiClearTooltip();

        // Open Core
        if (GuiButton((Rectangle){ padding, padding, buttonWidth, buttonHeight }, GuiIconText(RICON_FOLDER_FILE_OPEN, "Open Core")))
        {
            openFileDialog.fileDialogActive = true;
            openFileType = 0;
        }
        GuiLabel((Rectangle){ padding + buttonWidth + smallPadding, padding, buttonWidth, buttonHeight }, GetLibretroName());

        // Core Actions.
        if (IsLibretroReady()) {
            // Open Game
            if (GuiButton((Rectangle){ padding, buttonHeight + padding + smallPadding, buttonWidth, buttonHeight }, GuiIconText(RICON_FILETYPE_IMAGE, "Open Game"))) {
                openFileDialog.fileDialogActive = true;
                openFileType = 1;
            }

            // Run/Close Game
            Rectangle runResetGameRect = (Rectangle){ padding, buttonHeight * 2 + padding + smallPadding * 2, buttonWidth, buttonHeight };
            if (!IsLibretroGameReady()) {
                GuiSetTooltip("Run the core without content");
                if (GuiButton(runResetGameRect, GuiIconText(RICON_ARROW_RIGHT_FILL, "Run"))) {
                    if (LoadLibretroGame(NULL)) {
                        menuActive = false;
                    }
                }
                GuiClearTooltip();
            }
            else {
                if (GuiButton(runResetGameRect, GuiIconText(RICON_CROSS, "Close Game"))) {
                    UnloadLibretroGame();
                }
            }

            // Reset Game
            if (IsLibretroGameReady()) {
                if (GuiButton((Rectangle){ padding, buttonHeight * 3 + padding + smallPadding * 3, buttonWidth, buttonHeight }, GuiIconText(RICON_ROTATE_FILL, "Reset"))) {
                    ResetLibretro();
                    menuActive = false;
                }
            }
        }

        // Shaders
        if (IsLibretroGameReady()) {
            Rectangle shaderRect = (Rectangle){padding, GetScreenHeight() - padding - buttonHeight, buttonWidth, buttonHeight};
            currentShader = GuiToggleGroup(shaderRect, "Pixel Perfect;CRT;Scanlines", currentShader);
        }
    }

    // When the dialog is displayed, use a smaller font.
    GuiSetStyle(DEFAULT, TEXT_SIZE, 10);

    // Select a file.
    GuiFileDialog(&openFileDialog);
    if (openFileDialog.SelectFilePressed) {
        openFileDialog.SelectFilePressed = false;
        switch (openFileType) {
            case 0:
                TraceLog(LOG_INFO, "LIBRETRO: Open Core %s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText);
                InitLibretro(TextFormat("%s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText));
            break;
            case 1:
                TraceLog(LOG_INFO, "LIBRETRO: Open Content %s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText);
                if (LoadLibretroGame(TextFormat("%s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText))) {
                    menuActive = false;
                }
            break;
        }
    }
}

#endif
