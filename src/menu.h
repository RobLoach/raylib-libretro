#ifndef RAYLIB_LIBRETRO_MENU
#define RAYLIB_LIBRETRO_MENU
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
//#include "raygui.h"

#define GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION
#include "../examples/custom_file_dialog/gui_window_file_dialog.h"

#include "../include/raylib-libretro.h"
#include "shaders.h"

bool menuActive = true;
GuiWindowFileDialogState openFileDialog;
int openFileType = 0;
char* menuMessage;

void InitMenu() {
    // Set up the file dialog.
    openFileDialog = InitGuiWindowFileDialog(GetWorkingDirectory());
    TextCopy(openFileDialog.dirPathText, GetWorkingDirectory());

    // Configure the GUI styles.
    GuiLoadStyleDefault();

    // Cyber Style
    /*
    GuiSetStyle(0, 0, 0x2f7486ff);
    GuiSetStyle(0, 1, 0x024658ff);
    GuiSetStyle(0, 2, 0x51bfd3ff);
    GuiSetStyle(0, 3, 0x82cde0ff);
    GuiSetStyle(0, 4, 0x3299b4ff);
    GuiSetStyle(0, 5, 0xb6e1eaff);
    GuiSetStyle(0, 6, 0xeb7630ff);
    GuiSetStyle(0, 7, 0xffbc51ff);
    GuiSetStyle(0, 8, 0xd86f36ff);
    GuiSetStyle(0, 9, 0x134b5aff);
    GuiSetStyle(0, 10, 0x02313dff);
    GuiSetStyle(0, 11, 0x17505fff);
    GuiSetStyle(0, 16, 0x0000000e);
    GuiSetStyle(0, 17, 0x00000000);
    GuiSetStyle(0, 18, 0x81c0d0ff);
    GuiSetStyle(0, 19, 0x00222bff);
    */

    // Default style settings.
    GuiFade(WHITE, 0.95f);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 3);
}

void SetMenuActive(bool enabled) {
    menuActive = enabled;
}

bool IsMenuActive() {
    return menuActive;
}

bool IsFileCore(const char* filename) {
    return IsFileExtension(filename, ".so") ||
        IsFileExtension(filename, ".dll") ||
        IsFileExtension(filename, ".dylib") ||
        IsFileExtension(filename, ".3dsx") ||
        IsFileExtension(filename, ".cia");
}

/**
 * Handle any dropped files.
 */
void ManageDroppedFiles() {
    if (!IsFileDropped()) {
        return;
    }

    // Retrieve the list of dropped files.
    FilePathList files = LoadDroppedFiles();
    {
        // Load any dropped cores first.
        for (int i = 0; i < files.count; i++) {
            if (IsFileCore(files.paths[i])) {
                UnloadLibretroGame();
                CloseLibretro();
                // If it loaded successfully, stop trying to load any other cores.
                if (InitLibretro(files.paths[i])) {
                    break;
                }
            }
        }

        // Finally, load the found content.
        for (int i = 0; i < files.count; i++) {
            if (!IsFileCore(files.paths[i])) {
                // If loading worked, stop trying to load any other content.
                if (LoadLibretroGame(files.paths[i])) {
                    menuActive = false;
                    break;
                }
            }
        }
    }
    UnloadDroppedFiles(files);
}

void UpdateMenu() {
    ManageDroppedFiles();

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
    GuiPanel((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, "raylib-libretro");

    // On the main menu, have a large font.
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);

    // Status bar
    if (IsLibretroReady()) {
        const char* statusText = TextFormat("%s %s", GetLibretroName(), GetLibretroVersion());
        Rectangle statusBarRect = (Rectangle){0, GetScreenHeight() - 28, GetScreenWidth(), 28};
        GuiStatusBar(statusBarRect, statusText);
    }
    else {
        GuiLabel((Rectangle){padding + buttonWidth, padding, GetScreenWidth() - buttonWidth, buttonHeight}, " ...or Drag and Drop.");
        GuiDrawText("raylib-libretro", (Rectangle){0,0, GetScreenWidth(), GetScreenHeight()}, TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
    }

    if (!openFileDialog.windowActive) {

        // Close Menu
        //GuiSetTooltip("Close Menu (F1)");
        Rectangle closeButton = (Rectangle){GetScreenWidth() - buttonHeight - padding, padding, buttonHeight, buttonHeight};
        if (GuiButton(closeButton, GuiIconText(ICON_CROSS, ""))) {
            if (IsLibretroGameReady()) {
                menuActive = false;
                return;
            }
        }
        //GuiClearTooltip();

        // Fullscreen
        //GuiSetTooltip("Fullscreen (F11)");
        Rectangle fullscreenCheckbox = (Rectangle){GetScreenWidth() - buttonHeight * 2 - padding - smallPadding, padding, buttonHeight, buttonHeight};
        if (GuiButton(fullscreenCheckbox, GuiIconText(ICON_ZOOM_CENTER, ""))) {
            ToggleFullscreen();
        }
        //GuiClearTooltip();

        // Volume
        Rectangle muteCheckbox = (Rectangle){GetScreenWidth() - buttonHeight * 4 - (padding - smallPadding) * 2, padding, buttonHeight, buttonHeight};
        int muteIcon = (LibretroCore.volume >= 0.5f) ? ICON_AUDIO : ICON_WAVE;
        if (GuiButton(muteCheckbox, GuiIconText(muteIcon, ""))) {
            if (LibretroCore.volume <= 0.5f) {
                LibretroCore.volume = 1.0f;
            }
            else {
                LibretroCore.volume = 0.f;
            }
        }

        // Open Core
        //GuiSetTooltip("Select a libretro core to load");
        if (GuiButton((Rectangle){ padding, padding, buttonWidth, buttonHeight }, GuiIconText(ICON_FOLDER_FILE_OPEN, "Open Core")))
        {
            openFileDialog.windowActive = true;
            openFileType = 0;
        }
        //GuiClearTooltip();

        // Core Actions.
        if (IsLibretroReady()) {
            // Open Game
            //GuiSetTooltip("Select a game to run with the loaded core");
            if (GuiButton((Rectangle){ padding, buttonHeight + padding + smallPadding, buttonWidth, buttonHeight }, GuiIconText(ICON_FILETYPE_IMAGE, "Open Game"))) {
                openFileDialog.windowActive = true;
                openFileType = 1;
            }
            //GuiClearTooltip();

            // Run/Close Game
            Rectangle runResetGameRect = (Rectangle){ padding, buttonHeight * 2 + padding + smallPadding * 2, buttonWidth, buttonHeight };
            if (!IsLibretroGameReady()) {
                if (!DoesLibretroCoreNeedContent()) {
                    //GuiSetTooltip("Run the core without content");
                    if (GuiButton(runResetGameRect, GuiIconText(ICON_ARROW_RIGHT_FILL, "Run"))) {
                        if (LoadLibretroGame(NULL)) {
                            menuActive = false;
                        }
                    }
                    //GuiClearTooltip();
                }
            }
            else {
                //GuiSetTooltip("Stop the currently running game");
                if (GuiButton(runResetGameRect, GuiIconText(ICON_CROSS, "Close Game"))) {
                    UnloadLibretroGame();
                }
                //GuiClearTooltip();
            }

            // Reset Game
            if (IsLibretroGameReady()) {
                //GuiSetTooltip("Reset the currently running game");
                if (GuiButton((Rectangle){ padding, buttonHeight * 3 + padding + smallPadding * 3, buttonWidth, buttonHeight }, GuiIconText(ICON_ROTATE_FILL, "Reset"))) {
                    ResetLibretro();
                    menuActive = false;
                }
                //GuiClearTooltip();
            }
        }

        // Shaders
        if (IsLibretroGameReady()) {
            //GuiSetTooltip("Change shader (F10)");
            Rectangle shaderRect = (Rectangle){padding, GetScreenHeight() - padding - buttonHeight, buttonWidth, buttonHeight};
            currentShader = GuiToggleGroup(shaderRect, "Pixel Perfect;CRT;Scanlines", &currentShader);
            //GuiClearTooltip();
        }
    }

    // When the dialog is displayed, use a smaller font.
    GuiSetStyle(DEFAULT, TEXT_SIZE, 10);

    // Select a file.
    GuiWindowFileDialog(&openFileDialog);
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
