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
    openFileDialog.windowBounds.x = 10.0f;
    openFileDialog.windowBounds.y = 30.0f;
    openFileDialog.windowBounds.width = GetScreenWidth() - openFileDialog.windowBounds.x * 2.0f;
    openFileDialog.windowBounds.height = GetScreenHeight() - openFileDialog.windowBounds.y * 2.0f;
    TextCopy(openFileDialog.dirPathText, GetWorkingDirectory());

    // Configure the GUI styles.
    GuiLoadStyleDefault();

    // Dark Style
    GuiSetStyle(0, 0, 0x878787ff);
    GuiSetStyle(0, 1, 0x2c2c2cff);
    GuiSetStyle(0, 2, 0xc3c3c3ff);
    GuiSetStyle(0, 3, 0xe1e1e1ff);
    GuiSetStyle(0, 4, 0x848484ff);
    GuiSetStyle(0, 5, 0x181818ff);
    GuiSetStyle(0, 6, 0x000000ff);
    GuiSetStyle(0, 7, 0xefefefff);
    GuiSetStyle(0, 8, 0x202020ff);
    GuiSetStyle(0, 9, 0x6a6a6aff);
    GuiSetStyle(0, 10, 0x818181ff);
    GuiSetStyle(0, 11, 0x606060ff);
    GuiSetStyle(0, 16, 0x00000010);
    GuiSetStyle(0, 17, 0x00000000);
    GuiSetStyle(0, 18, 0x9d9d9dff);
    GuiSetStyle(0, 19, 0x3c3c3cff);
    GuiSetStyle(0, 20, 0x00000018);
    GuiSetStyle(1, 5, 0xf7f7f7ff);
    GuiSetStyle(1, 8, 0x898989ff);
    GuiSetStyle(4, 5, 0xb0b0b0ff);
    GuiSetStyle(5, 5, 0x848484ff);
    GuiSetStyle(9, 5, 0xf5f5f5ff);
    GuiSetStyle(10, 5, 0xf6f6f6ff);

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
        if (!openFileDialog.windowActive) {
            GuiLabel((Rectangle){padding + buttonWidth, padding, GetScreenWidth() - buttonWidth, buttonHeight}, " ...or Drag and Drop.");
        }
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
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);

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
