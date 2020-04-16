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

void InitMenu() {
    //GuiSetStyle(DEFAULT, TEXT_SIZE, 18);

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
    int buttonWidth = 150;
    int buttonHeight = 50;
    int padding = 50;
    int smallPadding = 25;

    GuiPanel((Rectangle){0,0, GetScreenWidth(), GetScreenHeight()});

    if (!openFileDialog.fileDialogActive) {
        // Close Menu
        if (IsLibretroGameReady()) {
            if (GuiButton((Rectangle){GetScreenWidth() - buttonWidth - padding, padding, buttonWidth, buttonHeight}, GuiIconText(RICON_CROSS, "Close Menu"))) {
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
        GuiLabel((Rectangle){ padding + buttonWidth + smallPadding, padding, buttonWidth, buttonHeight }, TextFormat("%s %s", GetLibretroName(), GetLibretroVersion()));

        if (IsLibretroReady()) {
            // Open Game
            if (GuiButton((Rectangle){ padding, buttonHeight + padding + smallPadding, buttonWidth, buttonHeight }, GuiIconText(RICON_FILETYPE_IMAGE, "Open Content")))
            {
                openFileDialog.fileDialogActive = true;
                openFileType = 1;
            }
        }
    }

    // Select a file.
    GuiFileDialog(&openFileDialog);
    if (openFileDialog.SelectFilePressed) {
        switch (openFileType) {
            case 0:
                TraceLog(LOG_INFO, "Open Core %s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText);
                openFileDialog.SelectFilePressed = false;
                InitLibretro(TextFormat("%s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText));
            break;
            case 1:
                TraceLog(LOG_INFO, "Open Content %s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText);
                openFileDialog.SelectFilePressed = false;
                if (LoadLibretroGame(TextFormat("%s/%s", openFileDialog.dirPathText, openFileDialog.fileNameText))) {
                    menuActive = false;
                }
            break;
        }
    }
}

#endif
