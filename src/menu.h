#ifndef RAYLIB_LIBRETRO_MENU
#define RAYLIB_LIBRETRO_MENU
#include "raylib.h"

#include <vector>
#include <string>

#include "../include/rlibretro.h"
#include "shaders.h"

//#include "../vendor/raygui/examples/custom_file_dialog/gui_file_dialog.h"

class Menu {
    public:
    bool menuActive = true;
    GuiFileDialogState openFileDialog;
    int openFileType = 0;
    std::string menuMessage;
    Shaders* shaders;

    Menu(Shaders* shadersManager) {
        shaders = shadersManager;
        // Set up the file dialog.
        openFileDialog = InitGuiFileDialog(GetScreenWidth() * 0.8f, GetScreenHeight() * 0.8f, GetWorkingDirectory(), false);
        TextCopy(openFileDialog.dirPathText, GetWorkingDirectory());

        // Configure the GUI styles.
        GuiLoadStyleDefault();

        // Cyber Style
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

        // Default style settings.
        GuiFade(0.9f);
        GuiSetStyle(DEFAULT, TEXT_SPACING, 3);
    }

    void SetMenuActive(bool enabled) {
        menuActive = enabled;
    }

    bool IsMenuActive() {
        return menuActive;
    }

    /**
     * Handle any dropped files.
     */
    void ManageDroppedFiles() {
        if (!IsFileDropped()) {
            return;
        }

        // Retrieve the list of dropped files.
        int filesCount;
        char **droppedFiles = GetDroppedFiles(&filesCount);
        {
            // Load any dropped cores first.
            for (int i = 0; i < filesCount; i++) {
                if (IsFileExtension(droppedFiles[i], ".so") ||
                        IsFileExtension(droppedFiles[i], ".dll") ||
                        IsFileExtension(droppedFiles[i], ".3dsx") ||
                        IsFileExtension(droppedFiles[i], ".cia")) {
                    UnloadLibretroGame();
                    CloseLibretro();
                    // If it loaded successfully, stop trying to load any other cores.
                    if (InitLibretro(droppedFiles[i])) {
                        break;
                    }
                }
            }

            // Finally, load the found content.
            for (int i = 0; i < filesCount; i++) {
                if (!IsFileExtension(droppedFiles[i], ".so") && !IsFileExtension(droppedFiles[i], ".dll")) {
                    // If loading worked, stop trying to load any other content.
                    if (LoadLibretroGame(droppedFiles[i])) {
                        menuActive = false;
                        break;
                    }
                }
            }
        }
        ClearDroppedFiles();
    }

    void Update() {
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
        GuiPanel((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});

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
            GuiDrawText("raylib-libretro", (Rectangle){0,0, GetScreenWidth(), GetScreenHeight()}, GUI_TEXT_ALIGN_CENTER, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)));
        }

        if (!openFileDialog.fileDialogActive) {

            // Close Menu
            GuiSetTooltip("Close Menu (F1)");
            Rectangle closeButton = (Rectangle){GetScreenWidth() - buttonHeight - padding, padding, buttonHeight, buttonHeight};
            if (GuiButton(closeButton, GuiIconText(RICON_CROSS, ""))) {
                if (IsLibretroGameReady()) {
                    menuActive = false;
                    return;
                }
            }
            GuiClearTooltip();

            // Fullscreen
            GuiSetTooltip("Fullscreen (F11)");
            Rectangle fullscreenCheckbox = (Rectangle){GetScreenWidth() - buttonHeight * 2 - padding - smallPadding, padding, buttonHeight, buttonHeight};
            if (GuiButton(fullscreenCheckbox, GuiIconText(RICON_ZOOM_CENTER, ""))) {
                ToggleFullscreen();
            }
            GuiClearTooltip();

            // Open Core
            GuiSetTooltip("Select a libretro core to load");
            if (GuiButton((Rectangle){ padding, padding, buttonWidth, buttonHeight }, GuiIconText(RICON_FOLDER_FILE_OPEN, "Open Core")))
            {
                openFileDialog.fileDialogActive = true;
                openFileType = 0;
            }
            GuiClearTooltip();

            // Core Actions.
            if (IsLibretroReady()) {
                // Open Game
                GuiSetTooltip("Select a game to run with the loaded core");
                if (GuiButton((Rectangle){ padding, buttonHeight + padding + smallPadding, buttonWidth, buttonHeight }, GuiIconText(RICON_FILETYPE_IMAGE, "Open Game"))) {
                    openFileDialog.fileDialogActive = true;
                    openFileType = 1;
                }
                GuiClearTooltip();

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
                    GuiSetTooltip("Stop the currently running game");
                    if (GuiButton(runResetGameRect, GuiIconText(RICON_CROSS, "Close Game"))) {
                        UnloadLibretroGame();
                    }
                    GuiClearTooltip();
                }

                // Reset Game
                if (IsLibretroGameReady()) {
                    GuiSetTooltip("Reset the currently running game");
                    if (GuiButton((Rectangle){ padding, buttonHeight * 3 + padding + smallPadding * 3, buttonWidth, buttonHeight }, GuiIconText(RICON_ROTATE_FILL, "Reset"))) {
                        ResetLibretro();
                        menuActive = false;
                    }
                    GuiClearTooltip();
                }
            }

            // Shaders
            if (IsLibretroGameReady()) {
                GuiSetTooltip("Change shader (F10)");
                Rectangle shaderRect = (Rectangle){padding, GetScreenHeight() - padding - buttonHeight, buttonWidth, buttonHeight};
                shaders->currentShader = GuiToggleGroup(shaderRect, "Pixel Perfect;CRT;Scanlines", shaders->currentShader);
                GuiClearTooltip();
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
};

#endif
