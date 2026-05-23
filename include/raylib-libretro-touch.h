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

#define RAYLIB_LIBRETRO_TOUCH_BUTTON_COUNT 12

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

#include "raymath.h"

#if defined(__cplusplus)
extern "C" {
#endif

// All sizes/positions are derived from a single reference unit so the layout
// stays consistent across aspect ratios. The unit is a fraction of the smaller
// screen dimension; buttons are square in that unit. The D-pad and face button
// clusters are each 3x3 grids: D-pad uses a cross, face buttons use a diamond.
static int LibretroTouchBuildButtons(TouchControlsButton* btns, int w, int h) {
    float ref  = (float)((w < h) ? w : h);
    float bs   = ref * 0.12f;   // D-pad / select / start button size
    float fbs  = ref * 0.15f;   // face button size
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

    // Face buttons: bottom-right, larger, SNES diamond
    //   . X .
    //   Y . A
    //   . B .
    float fsp = fbs * 0.82f;  // center-to-center spacing (< fbs = slight overlap)
    float fy = h - (2*fsp + fbs) - edge;
    float fx = w - (2*fsp + fbs) - w * 0.04f;
    btns[n++] = (TouchControlsButton){{ fx + fsp,     fy,         fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_X, "X", DARKBLUE  };
    btns[n++] = (TouchControlsButton){{ fx,            fy + fsp,   fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_Y, "Y", DARKGREEN };
    btns[n++] = (TouchControlsButton){{ fx + 2*fsp,    fy + fsp,   fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_A, "A", MAROON    };
    btns[n++] = (TouchControlsButton){{ fx + fsp,      fy + 2*fsp, fbs, fbs }, RETRO_DEVICE_ID_JOYPAD_B, "B", GOLD      };

    // Select / Start: bottom center with a small gap between them
    float selGap = ref * 0.01f;
    float cx = (w - (2*bs + selGap)) * 0.5f;
    float cy = h - bs - edge;
    btns[n++] = (TouchControlsButton){{ cx,               cy, bs, bs }, RETRO_DEVICE_ID_JOYPAD_SELECT, "SEL", GRAY };
    btns[n++] = (TouchControlsButton){{ cx + bs + selGap, cy, bs, bs }, RETRO_DEVICE_ID_JOYPAD_START,  "STA", GRAY };

    // Shoulder buttons: both on the right, side by side above the face buttons
    float shGap = ref * 0.01f;
    float shW   = (3*bs - shGap) * 0.5f;
    float shY   = fy - bs - ref * 0.02f;
    btns[n++] = (TouchControlsButton){{ fx,              shY, shW, bs }, RETRO_DEVICE_ID_JOYPAD_L, "L", DARKGRAY };
    btns[n++] = (TouchControlsButton){{ fx + shW + shGap, shY, shW, bs }, RETRO_DEVICE_ID_JOYPAD_R, "R", DARKGRAY };
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
    return id == RETRO_DEVICE_ID_JOYPAD_A || id == RETRO_DEVICE_ID_JOYPAD_B ||
           id == RETRO_DEVICE_ID_JOYPAD_X || id == RETRO_DEVICE_ID_JOYPAD_Y;
}

static bool LibretroTouchIsDpadButton(int id) {
    return id == RETRO_DEVICE_ID_JOYPAD_UP   || id == RETRO_DEVICE_ID_JOYPAD_DOWN ||
           id == RETRO_DEVICE_ID_JOYPAD_LEFT || id == RETRO_DEVICE_ID_JOYPAD_RIGHT;
}


static Rectangle LibretroTouchMenuRect(int w, int h) {
    float ref = (float)((w < h) ? w : h);
    float bs  = ref * 0.10f;
    float margin = ref * 0.02f;
    Rectangle r = { (float)w - bs - margin, margin, bs, bs };
    return r;
}

// Returns the raylib keyboard key mapped to a joypad button for port 0.
static int LibretroTouchJoypadKeyboard(int buttonId) {
    switch (buttonId) {
        case RETRO_DEVICE_ID_JOYPAD_UP:     return KEY_UP;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   return KEY_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   return KEY_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return KEY_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_B:      return KEY_Z;
        case RETRO_DEVICE_ID_JOYPAD_A:      return KEY_X;
        case RETRO_DEVICE_ID_JOYPAD_Y:      return KEY_A;
        case RETRO_DEVICE_ID_JOYPAD_X:      return KEY_S;
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return KEY_RIGHT_SHIFT;
        case RETRO_DEVICE_ID_JOYPAD_START:  return KEY_ENTER;
        case RETRO_DEVICE_ID_JOYPAD_L:      return KEY_Q;
        case RETRO_DEVICE_ID_JOYPAD_R:      return KEY_W;
        case RETRO_DEVICE_ID_JOYPAD_L2:     return KEY_E;
        case RETRO_DEVICE_ID_JOYPAD_R2:     return KEY_R;
        case RETRO_DEVICE_ID_JOYPAD_L3:     return KEY_D;
        case RETRO_DEVICE_ID_JOYPAD_R3:     return KEY_F;
        default: return KEY_NULL;
    }
}

// Returns the raylib GamepadButton mapped to a joypad button for port 0.
static int LibretroTouchJoypadGamepad(int buttonId) {
    switch (buttonId) {
        case RETRO_DEVICE_ID_JOYPAD_UP:     return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_B:      return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_A:      return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_Y:      return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_X:      return GAMEPAD_BUTTON_RIGHT_FACE_UP;
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return GAMEPAD_BUTTON_MIDDLE_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_START:  return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_L:      return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case RETRO_DEVICE_ID_JOYPAD_R:      return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
        case RETRO_DEVICE_ID_JOYPAD_L2:     return GAMEPAD_BUTTON_LEFT_TRIGGER_2;
        case RETRO_DEVICE_ID_JOYPAD_R2:     return GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
        case RETRO_DEVICE_ID_JOYPAD_L3:     return GAMEPAD_BUTTON_LEFT_THUMB;
        case RETRO_DEVICE_ID_JOYPAD_R3:     return GAMEPAD_BUTTON_RIGHT_THUMB;
        default: return GAMEPAD_BUTTON_UNKNOWN;
    }
}

// Returns true if the joypad button is currently held via keyboard or gamepad.
static bool LibretroTouchIsPhysicalButtonDown(int buttonId) {
    int key = LibretroTouchJoypadKeyboard(buttonId);
    if (key != KEY_NULL && IsKeyDown(key)) return true;
    if (IsGamepadAvailable(0)) {
        int gbtn = LibretroTouchJoypadGamepad(buttonId);
        if (gbtn != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(0, gbtn)) return true;
    }
    return false;
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

// Computes the center and radius of the circular D-pad from the 4 cardinal rects.
static void LibretroTouchDpadInfo(Vector2 *center, float *radius) {
    int iUp    = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_UP);
    int iDown  = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_DOWN);
    int iLeft  = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_LEFT);
    int iRight = LibretroTouchFindButtonById(RETRO_DEVICE_ID_JOYPAD_RIGHT);
    Rectangle left  = LibretroTouchButtons[iLeft].rect;
    Rectangle right = LibretroTouchButtons[iRight].rect;
    Rectangle up    = LibretroTouchButtons[iUp].rect;
    Rectangle down  = LibretroTouchButtons[iDown].rect;
    center->x = left.x + (right.x + right.width  - left.x) * 0.5f;
    center->y = up.y   + (down.y  + down.height  - up.y)   * 0.5f;
    *radius   = (right.x + right.width - left.x) * 0.5f;
}

// Track whether the touch/mouse press that just landed on the MENU button has
// been released. The menu opens on release, not press — opening on press lets
// the press leak into Nuklear on the next frame and immediately triggers the
// first menu item (Resume), closing the menu instantly.
static bool LibretroTouchMenuArmed = false;
static bool LibretroTouchMenuTriggered = false;
static bool LibretroTouchHapticsEnabled = true;
// D-pad lock: once a press begins inside the circle, the direction is tracked
// from that input point even after it moves outside the circle.
// dpadTouchId == -1 means the lock belongs to the mouse; -2 means no lock.
static bool    LibretroTouchDpadLocked   = false;
static int     LibretroTouchDpadTouchId  = -2;
static Vector2 LibretroTouchDpadLockedPos = {0};

// Idle fade: alpha trends toward 0.25 after 3 s of no touch activity.
static double LibretroTouchLastActiveTime = -1.0;  // -1 = never touched
static float  LibretroTouchAlpha = 1.0f;

static Color LibretroTouchFadeColor(Color c, float a) {
    c.a = (unsigned char)(c.a * a);
    return c;
}

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

    // D-pad: 8-direction circular input with press-locking.
    // Once a press begins inside the circle the direction is tracked from that
    // input point even after it moves outside — angle from center determines direction.
    {
        Vector2 dc; float dr;
        LibretroTouchDpadInfo(&dc, &dr);
        static const float PI8 = 0.3926991f;  // PI/8

        // Acquire lock on the initial press that lands inside the circle.
        if (!LibretroTouchDpadLocked) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                    CheckCollisionPointCircle(GetMousePosition(), dc, dr)) {
                LibretroTouchDpadLocked = true;
                LibretroTouchDpadTouchId = -1;
            }
            for (int t = 0; t < touchCount && !LibretroTouchDpadLocked; t++) {
                if (CheckCollisionPointCircle(GetTouchPosition(t), dc, dr)) {
                    LibretroTouchDpadLocked = true;
                    LibretroTouchDpadTouchId = GetTouchPointId(t);
                }
            }
        }

        // While locked, resolve direction from the locked input's current position.
        if (LibretroTouchDpadLocked) {
            Vector2 pos = {0}; bool found = false;
            if (LibretroTouchDpadTouchId == -1) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { pos = GetMousePosition(); found = true; }
                else LibretroTouchDpadLocked = false;
            } else {
                for (int t = 0; t < touchCount; t++) {
                    if (GetTouchPointId(t) == LibretroTouchDpadTouchId) {
                        pos = GetTouchPosition(t); found = true; break;
                    }
                }
                if (!found) LibretroTouchDpadLocked = false;
            }
            if (found) {
                LibretroTouchDpadLockedPos = pos;
                float ddx = pos.x - dc.x, ddy = pos.y - dc.y;
                if (Vector2Length((Vector2){ddx, ddy}) > dr * 0.15f) {  // dead zone
                    float angle = atan2f(ddy, ddx);
                    if      (angle >= -PI8   && angle <  PI8)    LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_RIGHT] = true;
                    else if (angle >=  PI8   && angle <  3*PI8)  { LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_DOWN]  = true; LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_RIGHT] = true; }
                    else if (angle >=  3*PI8 && angle <  5*PI8)  LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_DOWN]  = true;
                    else if (angle >=  5*PI8 && angle <  7*PI8)  { LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_DOWN]  = true; LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_LEFT]  = true; }
                    else if (angle >= -7*PI8 && angle < -5*PI8)  { LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_UP]   = true; LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_LEFT]  = true; }
                    else if (angle >= -5*PI8 && angle < -3*PI8)  LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_UP]   = true;
                    else if (angle >= -3*PI8 && angle < -PI8)    { LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_UP]   = true; LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_RIGHT] = true; }
                    else                                          LibretroCore.virtualJoypadState[RETRO_DEVICE_ID_JOYPAD_LEFT]  = true;
                }
            }
        }
    }

    // Non-D-pad buttons: rect collision.
    for (int i = 0; i < LibretroTouchButtonsCount; i++) {
        if (LibretroTouchIsDpadButton(LibretroTouchButtons[i].buttonId)) continue;
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

    // Refresh idle timer whenever any touch input is active.
    bool anyTouchActive = LibretroTouchDpadLocked || LibretroTouchMenuArmed;
    if (!anyTouchActive) {
        for (int i = 0; i < 16; i++) {
            if (LibretroCore.virtualJoypadState[i]) { anyTouchActive = true; break; }
        }
    }
    if (anyTouchActive) LibretroTouchLastActiveTime = GetTime();
}

bool IsTouchControlsMenuPressed(void) {
    return LibretroTouchMenuTriggered;
}

void SetTouchHapticsEnabled(bool enabled) { LibretroTouchHapticsEnabled = enabled; }
bool GetTouchHapticsEnabled(void) { return LibretroTouchHapticsEnabled; }

void DrawTouchControls(void) {
    LibretroTouchEnsureLayout();

    // Idle fade: smoothly trend toward 25% opacity after 3 s of no touch input.
    {
        double idleSecs = (LibretroTouchLastActiveTime >= 0.0)
            ? (GetTime() - LibretroTouchLastActiveTime) : 0.0;
        float target = (idleSecs > 3.0) ? 0.25f : 1.0f;
        LibretroTouchAlpha = Lerp(LibretroTouchAlpha, target, GetFrameTime() * 4.0f);
    }
    float alpha = LibretroTouchAlpha;

    // D-pad: filled circle with 8 directional arrow indicators.
    // Arrows are drawn with tip pointing outward; a triangle whose base sits at
    // arrowDist and whose tip is arrowDist+arrowSize from the center.
    // DrawTriangle uses screen-space CCW winding: tip, then right-base, left-base.
    {
        // angle, primary button, secondary button (-1 = cardinal, only one required)
        static const struct { float angle; int b1, b2; } dirs[8] = {
            {  0.0000f, RETRO_DEVICE_ID_JOYPAD_RIGHT, -1                            },
            {  0.7854f, RETRO_DEVICE_ID_JOYPAD_DOWN,  RETRO_DEVICE_ID_JOYPAD_RIGHT },
            {  1.5708f, RETRO_DEVICE_ID_JOYPAD_DOWN,  -1                            },
            {  2.3562f, RETRO_DEVICE_ID_JOYPAD_DOWN,  RETRO_DEVICE_ID_JOYPAD_LEFT  },
            {  3.1416f, RETRO_DEVICE_ID_JOYPAD_LEFT,  -1                            },
            { -2.3562f, RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_LEFT  },
            { -1.5708f, RETRO_DEVICE_ID_JOYPAD_UP,    -1                            },
            { -0.7854f, RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_RIGHT },
        };

        Vector2 dc; float dr;
        LibretroTouchDpadInfo(&dc, &dr);

        DrawCircleV(dc, dr, LibretroTouchFadeColor((Color){ 40, 40, 40, 160 }, alpha));

        float arrowDist = dr * 0.60f;
        float arrowSize = dr * 0.22f;

        for (int d = 0; d < 8; d++) {
            bool b1 = LibretroCore.virtualJoypadState[dirs[d].b1] || LibretroTouchIsPhysicalButtonDown(dirs[d].b1);
            bool b2 = (dirs[d].b2 < 0) || LibretroCore.virtualJoypadState[dirs[d].b2] || LibretroTouchIsPhysicalButtonDown(dirs[d].b2);
            Color arrowColor = LibretroTouchFadeColor(
                (b1 && b2) ? (Color){ 255, 255, 255, 230 } : (Color){ 180, 180, 180, 100 }, alpha);
            float ca = cosf(dirs[d].angle), sa = sinf(dirs[d].angle);
            Vector2 tip = { dc.x + ca*(arrowDist + arrowSize), dc.y + sa*(arrowDist + arrowSize) };
            Vector2 rb  = { dc.x + ca*arrowDist + sa*arrowSize, dc.y + sa*arrowDist - ca*arrowSize };
            Vector2 lb  = { dc.x + ca*arrowDist - sa*arrowSize, dc.y + sa*arrowDist + ca*arrowSize };
            DrawTriangle(tip, rb, lb, arrowColor);
        }

        DrawCircleV(dc, dr * 0.18f, LibretroTouchFadeColor((Color){ 60, 60, 60, 200 }, alpha));

        // Direction indicator: a dot on the inner edge showing where the locked
        // press is pointing. Hidden when in the dead zone or not locked.
        if (LibretroTouchDpadLocked) {
            float ddx = LibretroTouchDpadLockedPos.x - dc.x;
            float ddy = LibretroTouchDpadLockedPos.y - dc.y;
            float dist = Vector2Length((Vector2){ddx, ddy});
            if (dist > dr * 0.15f) {
                Vector2 dir = { ddx / dist, ddy / dist };
                Vector2 indicatorPos = { dc.x + dir.x * dr * 0.82f, dc.y + dir.y * dr * 0.82f };
                DrawCircleV(indicatorPos, dr * 0.12f,
                    LibretroTouchFadeColor((Color){ 255, 255, 255, 220 }, alpha));
            }
        }
    }

    for (int i = 0; i < LibretroTouchButtonsCount; i++) {
        TouchControlsButton* btns = LibretroTouchButtons;
        int id = btns[i].buttonId;
        if (LibretroTouchIsDpadButton(id)) continue;  // drawn separately as circular D-pad
        bool isMenuLike = (id == RETRO_DEVICE_ID_JOYPAD_SELECT || id == RETRO_DEVICE_ID_JOYPAD_START);
        bool touchHeld    = LibretroCore.virtualJoypadState[id];
        bool physicalHeld = LibretroTouchIsPhysicalButtonDown(id);
        Color c = btns[i].color;
        unsigned char restA = isMenuLike ? 70 : 140;
        unsigned char heldA = isMenuLike ? 160 : 220;
        c.a = (unsigned char)(((touchHeld || physicalHeld) ? heldA : restA) * alpha);
        float fcx = btns[i].rect.x + btns[i].rect.width  * 0.5f;
        float fcy = btns[i].rect.y + btns[i].rect.height * 0.5f;
        if (LibretroTouchIsCircleButton(id)) {
            DrawCircle((int)fcx, (int)fcy, btns[i].rect.width * 0.5f, c);
        } else {
            DrawRectangleRounded(btns[i].rect, 0.5f, 6, c);
        }
        Color textColor = LibretroTouchFadeColor(WHITE, alpha);
        if (isMenuLike) textColor.a = (unsigned char)(180 * alpha);
        Font font = LibretroTouchFont();
        float fs = btns[i].rect.height * 0.4f;
        Vector2 ts = MeasureTextEx(font, btns[i].label, fs, 1.0f);
        DrawTextEx(font, btns[i].label,
            (Vector2){ fcx - ts.x * 0.5f, fcy - ts.y * 0.5f },
            fs, 1.0f, textColor);
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
    Color gc = hovered ? LibretroTouchFadeColor(GRAY, alpha)
                       : LibretroTouchFadeColor((Color){80, 80, 80, 160}, alpha);
    DrawRectangleRounded(guide, 0.3f, 4, gc);

    float barW = guide.width  * 0.50f;
    float barH = guide.height * 0.10f;
    float gap  = guide.height * 0.20f;
    float gcx  = guide.x + guide.width  * 0.5f;
    float gcy  = guide.y + guide.height * 0.5f;
    float bx   = gcx - barW * 0.5f;
    for (int row = -1; row <= 1; row++) {
        Rectangle bar = { bx, gcy + row * gap - barH * 0.5f, barW, barH };
        DrawRectangleRounded(bar, 1.0f, 4, LibretroTouchFadeColor(WHITE, alpha));
    }
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION
