/**********************************************************************************************
*
*   raylib-libretro-touch.h - On-screen/touch controls for raylib-libretro.
*
*   USAGE:
*       #define RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION
*       #include "raylib-libretro-touch.h"
*
*   Renders a virtual gamepad (D-pad, face buttons, Select/Start, Menu) and
*   feeds presses into LibretroCore.virtualJoypadState. Sizing is derived
*   from the smaller screen dimension so buttons stay square and consistent
*   regardless of aspect ratio or window size.
*
*   LICENSE: zlib/libpng
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_TOUCH_H
#define RAYLIB_LIBRETRO_TOUCH_H

#include "raylib.h"
#include "raylib-libretro.h"  // RETRO_DEVICE_ID_JOYPAD_*, LibretroCore

#define RAYLIB_LIBRETRO_TOUCH_BUTTON_COUNT 10

typedef struct TouchControlsButton {
    Rectangle rect;     // screen-space rectangle (pixels)
    int buttonId;       // RETRO_DEVICE_ID_JOYPAD_*
    const char* label;
    Color color;
} TouchControlsButton;

#if defined(__cplusplus)
extern "C" {
#endif

void UpdateTouchControls(void);
void DrawTouchControls(void);
bool IsTouchControlsMenuPressed(void);
void SetTouchHapticsEnabled(bool enabled);
bool GetTouchHapticsEnabled(void);

#if defined(__cplusplus)
}
#endif

#endif /* RAYLIB_LIBRETRO_TOUCH_H */

#ifdef RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION_ONCE

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// All sizes/positions are derived from a single reference unit so the layout
// stays consistent across aspect ratios. The unit is a fraction of the smaller
// screen dimension; buttons are square in that unit. The D-pad and face button
// clusters are each 3x3 grids: D-pad uses a cross, face buttons use a diamond.
static int LibretroTouchBuildButtons(TouchControlsButton* btns, int w, int h) {
    float ref  = (float)((w < h) ? w : h);
    float bs   = ref * 0.10f;   // D-pad / select / start button size
    float fbs  = ref * 0.12f;   // face button size (larger for easier tapping)
    float edge = ref * 0.02f;   // margin from screen edges

    // D-pad: bottom-left, cross layout, buttons touching (no gap)
    //   . U .
    //   L . R
    //   . D .
    float dy = h - 3*bs - edge;
    float dx = w * 0.04f;
    int n = 0;
    btns[n++] = (TouchControlsButton){{ dx + bs,     dy,        bs, bs }, RETRO_DEVICE_ID_JOYPAD_UP,    "^", DARKGRAY };
    btns[n++] = (TouchControlsButton){{ dx,           dy + bs,   bs, bs }, RETRO_DEVICE_ID_JOYPAD_LEFT,  "<", DARKGRAY };
    btns[n++] = (TouchControlsButton){{ dx + 2*bs,    dy + bs,   bs, bs }, RETRO_DEVICE_ID_JOYPAD_RIGHT, ">", DARKGRAY };
    btns[n++] = (TouchControlsButton){{ dx + bs,      dy + 2*bs, bs, bs }, RETRO_DEVICE_ID_JOYPAD_DOWN,  "v", DARKGRAY };

    // Face buttons: bottom-right, larger, touching, SNES diamond
    //   . X .
    //   Y . A
    //   . B .
    float fy = h - 3*fbs - edge;
    float fx = w - 3*fbs - w * 0.04f;
    btns[n++] = (TouchControlsButton){{ fx + fbs,     fy,         fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_X, "X", DARKBLUE  };
    btns[n++] = (TouchControlsButton){{ fx,            fy + fbs,   fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_Y, "Y", DARKGREEN };
    btns[n++] = (TouchControlsButton){{ fx + 2*fbs,    fy + fbs,   fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_A, "A", MAROON    };
    btns[n++] = (TouchControlsButton){{ fx + fbs,      fy + 2*fbs, fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_B, "B", GOLD      };

    // Select / Start: bottom center with a small gap between them
    float selGap = ref * 0.01f;
    float cx = (w - (2*bs + selGap)) * 0.5f;
    float cy = h - bs - edge;
    btns[n++] = (TouchControlsButton){{ cx,               cy, bs, bs }, RETRO_DEVICE_ID_JOYPAD_SELECT, "SEL", GRAY };
    btns[n++] = (TouchControlsButton){{ cx + bs + selGap, cy, bs, bs }, RETRO_DEVICE_ID_JOYPAD_START,  "STA", GRAY };
    return n;
}

// Use the menu's loaded font when the menu header was included earlier in the
// same translation unit. Falls back to raylib's built-in font otherwise so
// touch.h remains usable standalone.
static Font LibretroTouchFont(void) {
#ifdef RAYLIB_LIBRETRO_MENU_H
    Font f = GetLibretroMenuFont();
    if (f.texture.id != 0) return f;
#endif
    return GetFontDefault();
}

static bool LibretroTouchIsCircleButton(int id) {
    return id == RETRO_DEVICE_ID_JOYPAD_UP    || id == RETRO_DEVICE_ID_JOYPAD_DOWN ||
           id == RETRO_DEVICE_ID_JOYPAD_LEFT  || id == RETRO_DEVICE_ID_JOYPAD_RIGHT ||
           id == RETRO_DEVICE_ID_JOYPAD_A     || id == RETRO_DEVICE_ID_JOYPAD_B ||
           id == RETRO_DEVICE_ID_JOYPAD_X     || id == RETRO_DEVICE_ID_JOYPAD_Y;
}

static bool LibretroTouchIsDpadButton(int id) {
    return id == RETRO_DEVICE_ID_JOYPAD_UP   || id == RETRO_DEVICE_ID_JOYPAD_DOWN ||
           id == RETRO_DEVICE_ID_JOYPAD_LEFT || id == RETRO_DEVICE_ID_JOYPAD_RIGHT;
}

// Draws a filled triangle arrow inside the button rect. Vertices are supplied
// to DrawTriangle in counter-clockwise order (screen space) so the triangle is
// not back-face culled.
static void LibretroTouchDrawDpadArrow(Rectangle r, int id, Color color) {
    float cx = r.x + r.width  * 0.5f;
    float cy = r.y + r.height * 0.5f;
    float s  = r.width * 0.22f;   // triangle half-extent
    Vector2 v1 = {0}, v2 = {0}, v3 = {0};
    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_UP:
            v1 = (Vector2){ cx,     cy - s };  // tip
            v2 = (Vector2){ cx - s, cy + s };  // bottom-left
            v3 = (Vector2){ cx + s, cy + s };  // bottom-right
            break;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            v1 = (Vector2){ cx,     cy + s };  // tip
            v2 = (Vector2){ cx + s, cy - s };  // top-right
            v3 = (Vector2){ cx - s, cy - s };  // top-left
            break;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            v1 = (Vector2){ cx - s, cy     };  // tip
            v2 = (Vector2){ cx + s, cy + s };  // bottom-right
            v3 = (Vector2){ cx + s, cy - s };  // top-right
            break;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            v1 = (Vector2){ cx + s, cy     };  // tip
            v2 = (Vector2){ cx - s, cy - s };  // top-left
            v3 = (Vector2){ cx - s, cy + s };  // bottom-left
            break;
        default: return;
    }
    DrawTriangle(v1, v2, v3, color);
}

static Rectangle LibretroTouchMenuRect(int w, int h) {
    float ref = (float)((w < h) ? w : h);
    float bs  = ref * 0.10f;
    float margin = ref * 0.02f;
    Rectangle r = { (float)w - bs - margin, margin, bs, bs };
    return r;
}

// Cached layout — rebuilt only when the screen size changes so a single frame
// pays for layout math once across Update + Draw.
static TouchControlsButton LibretroTouchButtons[RAYLIB_LIBRETRO_TOUCH_BUTTON_COUNT];
static int LibretroTouchButtonsCount = 0;
static int LibretroTouchCachedW = -1;
static int LibretroTouchCachedH = -1;

static void LibretroTouchEnsureLayout(void) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    if (w == LibretroTouchCachedW && h == LibretroTouchCachedH && LibretroTouchButtonsCount > 0) return;
    LibretroTouchButtonsCount = LibretroTouchBuildButtons(LibretroTouchButtons, w, h);
    LibretroTouchCachedW = w;
    LibretroTouchCachedH = h;
}

static int LibretroTouchFindButtonById(int id) {
    for (int i = 0; i < LibretroTouchButtonsCount; i++) {
        if (LibretroTouchButtons[i].buttonId == id) return i;
    }
    return -1;
}

// Track whether the touch/mouse press that just landed on the MENU button has
// been released. The menu opens on release, not press — opening on press lets
// the press leak into Nuklear on the next frame and immediately triggers the
// first menu item (Resume), closing the menu instantly.
static bool LibretroTouchMenuArmed = false;
static bool LibretroTouchMenuTriggered = false;
static bool LibretroTouchHapticsEnabled = true;

static void LibretroTouchTriggerHaptic(void) {
#if defined(PLATFORM_WEB)
    EM_ASM({ if (typeof navigator !== 'undefined' && navigator.vibrate) navigator.vibrate(15); });
#endif
}

void UpdateTouchControls(void) {
    LibretroTouchEnsureLayout();

    bool prevState[16] = {0};
    for (int i = 0; i < 16; i++) prevState[i] = LibretroCore.virtualJoypadState[i];
    bool prevMenuArmed = LibretroTouchMenuArmed;

    memset(LibretroCore.virtualJoypadState, 0, sizeof(LibretroCore.virtualJoypadState));

    Vector2 points[10];
    int nPoints = 0;
    int touchCount = GetTouchPointCount();
    for (int t = 0; t < touchCount && nPoints < 10; t++) {
        points[nPoints++] = GetTouchPosition(t);
    }
    // On the web a touch is also reported as a mouse press at the same
    // position; only sample the mouse when there are no active touches to
    // avoid registering the same point twice.
    if (touchCount == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && nPoints < 10) {
        points[nPoints++] = GetMousePosition();
    }

    for (int i = 0; i < LibretroTouchButtonsCount; i++) {
        for (int p = 0; p < nPoints; p++) {
            if (CheckCollisionPointRec(points[p], LibretroTouchButtons[i].rect)) {
                LibretroCore.virtualJoypadState[LibretroTouchButtons[i].buttonId] = true;
                break;
            }
        }
    }

    // Menu open-on-release detection. Arm when a press starts inside the menu
    // button; fire once when the press ends while still over the button.
    Rectangle guide = LibretroTouchMenuRect(LibretroTouchCachedW, LibretroTouchCachedH);
    LibretroTouchMenuTriggered = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), guide)) {
        LibretroTouchMenuArmed = true;
    }
    if (LibretroTouchMenuArmed && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(GetMousePosition(), guide)) {
            LibretroTouchMenuTriggered = true;
        }
        LibretroTouchMenuArmed = false;
    }

    if (IsGestureDetected(GESTURE_TAP)) {
        for (int t = 0; t < touchCount; t++) {
            if (CheckCollisionPointRec(GetTouchPosition(t), guide)) {
                LibretroTouchMenuTriggered = true;
                break;
            }
        }
    }

    if (LibretroTouchHapticsEnabled) {
        bool fired = false;
        for (int i = 0; i < LibretroTouchButtonsCount && !fired; i++) {
            int id = LibretroTouchButtons[i].buttonId;
            if (LibretroCore.virtualJoypadState[id] && !prevState[id]) {
                LibretroTouchTriggerHaptic();
                fired = true;
            }
        }
        if (!fired && LibretroTouchMenuArmed && !prevMenuArmed) {
            LibretroTouchTriggerHaptic();
        }
    }
}

bool IsTouchControlsMenuPressed(void) {
    return LibretroTouchMenuTriggered;
}

void SetTouchHapticsEnabled(bool enabled) { LibretroTouchHapticsEnabled = enabled; }
bool GetTouchHapticsEnabled(void) { return LibretroTouchHapticsEnabled; }

void DrawTouchControls(void) {
    LibretroTouchEnsureLayout();

    // D-pad unifying cross: connect the UP, LEFT, RIGHT, DOWN circles with a
    // single + shape behind them so the cluster reads as one D-pad. Look up by
    // buttonId so the cross still finds the right rects if the build order
    // ever changes.
    int iUp    = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_UP);
    int iDown  = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_DOWN);
    int iLeft  = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_LEFT);
    int iRight = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_RIGHT);
    if (iUp >= 0 && iDown >= 0 && iLeft >= 0 && iRight >= 0) {
        Rectangle up    = LibretroTouchButtons[iUp].rect;
        Rectangle down  = LibretroTouchButtons[iDown].rect;
        Rectangle left  = LibretroTouchButtons[iLeft].rect;
        Rectangle right = LibretroTouchButtons[iRight].rect;
        float dpadCx = left.x + (right.x + right.width - left.x) * 0.5f;
        float dpadCy = up.y + (down.y + down.height - up.y) * 0.5f;
        float armW = (right.x + right.width) - left.x;
        float armH = (down.y + down.height) - up.y;
        float bandThickness = up.width;
        Rectangle hBand = { left.x, dpadCy - bandThickness * 0.5f, armW, bandThickness };
        Rectangle vBand = { dpadCx - bandThickness * 0.5f, up.y,    bandThickness, armH };
        Color dpadBase = { 40, 40, 40, 160 };
        DrawRectangleRounded(hBand, 0.3f, 6, dpadBase);
        DrawRectangleRounded(vBand, 0.3f, 6, dpadBase);
    }

    for (int i = 0; i < LibretroTouchButtonsCount; i++) {
        TouchControlsButton* btns = LibretroTouchButtons;
        int id = btns[i].buttonId;
        bool isMenuLike = (id == RETRO_DEVICE_ID_JOYPAD_SELECT || id == RETRO_DEVICE_ID_JOYPAD_START);
        Color c = btns[i].color;
        unsigned char restA = isMenuLike ? 70 : 140;
        unsigned char heldA = isMenuLike ? 160 : 220;
        c.a = LibretroCore.virtualJoypadState[id] ? heldA : restA;
        float fcx = btns[i].rect.x + btns[i].rect.width  * 0.5f;
        float fcy = btns[i].rect.y + btns[i].rect.height * 0.5f;
        if (LibretroTouchIsCircleButton(id)) {
            DrawCircle((int)fcx, (int)fcy, btns[i].rect.width * 0.5f, c);
        } else {
            DrawRectangleRounded(btns[i].rect, 0.5f, 6, c);
        }
        Color textColor = WHITE;
        if (isMenuLike) textColor.a = 180;
        if (LibretroTouchIsDpadButton(id)) {
            LibretroTouchDrawDpadArrow(btns[i].rect, id, textColor);
        } else {
            Font font = LibretroTouchFont();
            float fs = btns[i].rect.height * 0.4f;
            Vector2 ts = MeasureTextEx(font, btns[i].label, fs, 1.0f);
            DrawTextEx(font, btns[i].label,
                (Vector2){ fcx - ts.x * 0.5f, fcy - ts.y * 0.5f },
                fs, 1.0f, textColor);
        }
    }

    // Menu button
    Rectangle guide = LibretroTouchMenuRect(LibretroTouchCachedW, LibretroTouchCachedH);
    bool hovered = false;
    int touchCount = GetTouchPointCount();
    for (int t = 0; t < touchCount; t++) {
        if (CheckCollisionPointRec(GetTouchPosition(t), guide)) { hovered = true; break; }
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), guide)) {
        hovered = true;
    }
    Color gc = hovered ? GRAY : (Color){80, 80, 80, 160};
    DrawRectangleRounded(guide, 0.3f, 4, gc);

    // Hamburger icon: three horizontal bars centered in the button.
    float barW = guide.width  * 0.50f;
    float barH = guide.height * 0.10f;
    float gap  = guide.height * 0.20f;
    float gcx  = guide.x + guide.width  * 0.5f;
    float gcy  = guide.y + guide.height * 0.5f;
    float bx   = gcx - barW * 0.5f;
    for (int row = -1; row <= 1; row++) {
        Rectangle bar = { bx, gcy + row * gap - barH * 0.5f, barW, barH };
        DrawRectangleRounded(bar, 1.0f, 4, WHITE);
    }
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION
