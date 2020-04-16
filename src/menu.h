#ifndef RAYLIB_LIBRETRO_MENU
#define RAYLIB_LIBRETRO_MENU
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include "raygui.h"

#define GUI_FILE_DIALOG_IMPLEMENTATION
#include "../examples/custom_file_dialog/gui_file_dialog.h"

#include "../include/rlibretro.h"

bool menuActive;
GuiFileDialogState openFileDialog;
int openFileType = 0;
char* menuMessage;

void InitMenu() {
    openFileDialog = InitGuiFileDialog(GetScreenWidth() * 0.8f, GetScreenHeight() * 0.8f, GetWorkingDirectory(), false);
    TextCopy(openFileDialog.dirPathText, GetWorkingDirectory());
    GuiFade(0.9f);
}

void UpdateMenu() {
    if (IsKeyReleased(KEY_F1)) {
        menuActive = !menuActive;
    }

    if (!menuActive) {
        return;
    }
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
        if (IsLibretroGameReady()) {
            if (GuiButton((Rectangle){GetScreenWidth() - buttonHeight - padding, padding, buttonHeight, buttonHeight}, GuiIconText(RICON_CROSS, ""))) {
                menuActive = false;
                return;
            }
        }

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
            if (GuiButton((Rectangle){ padding, buttonHeight + padding + smallPadding, buttonWidth, buttonHeight }, GuiIconText(RICON_FILETYPE_IMAGE, "Open Content"))) {
                openFileDialog.fileDialogActive = true;
                openFileType = 1;
            }

            // Run Game
            if (GuiButton((Rectangle){ padding, buttonHeight * 2 + padding + smallPadding * 2, buttonWidth, buttonHeight }, GuiIconText(RICON_ARROW_RIGHT_FILL, "Run"))) {
                if (LoadLibretroGame(NULL)) {
                    menuActive = false;
                }
            }
        }
    }
    else {
        // When the dialog is displayed, use a smaller font.
        GuiSetStyle(DEFAULT, TEXT_SIZE, 10);
        // Select a file.
        GuiFileDialog(&openFileDialog);
        if (openFileDialog.SelectFilePressed) {
            openFileDialog.SelectFilePressed = false;
            switch (openFileType) {
                case 0:
                    TraceLog(LOG_INFO, "Open Core %s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText);
                    InitLibretro(TextFormat("%s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText));
                break;
                case 1:
                    TraceLog(LOG_INFO, "Open Content %s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText);
                    if (LoadLibretroGame(TextFormat("%s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText))) {
                        menuActive = false;
                    }
                break;
            }
        }
    }

}

#endif
