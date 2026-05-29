/**********************************************************************************************
*
*   raylib-libretro-logo.h - The raylib-libretro logo as a generated raylib Image.
*
*   USAGE:
*       #define RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION
*       #include "raylib-libretro-logo.h"
*
*       Image logo = GetLibretroLogo();
*       Texture tex = LoadTextureFromImage(logo);
*       UnloadImage(logo);  // caller owns the returned Image
*
*   Builds the retro "space invader" mark (the same one used for the web/PWA icons)
*   pixel-by-pixel into an Image, so it needs no external asset files. Use
*   GetLibretroLogoEx() to control the pixel scale and colors.
*
*   LICENSE: zlib/libpng
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_LOGO_H
#define RAYLIB_LIBRETRO_LOGO_H

#include "raylib.h"

// Dimensions of the invader bitmap, in logo "pixels" (cells).
#define RAYLIB_LIBRETRO_LOGO_COLS 11
#define RAYLIB_LIBRETRO_LOGO_ROWS 8

#if defined(__cplusplus)
extern "C" {
#endif

// Build the logo with a custom cell size and colors. The returned Image is
// (RAYLIB_LIBRETRO_LOGO_COLS * scale) x (RAYLIB_LIBRETRO_LOGO_ROWS * scale).
// The caller owns the Image and must UnloadImage() it.
Image GetLibretroLogoEx(int scale, Color foreground, Color background);

// Build the logo at a sensible default size (the retro-green invader on a
// transparent background). The caller owns the Image and must UnloadImage() it.
Image GetLibretroLogo(void);

#if defined(__cplusplus)
}
#endif

#endif /* RAYLIB_LIBRETRO_LOGO_H */

#ifdef RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION_ONCE

#if defined(__cplusplus)
extern "C" {
#endif

// Classic space-invader bitmap (11 wide x 8 tall). '1' = filled cell.
// Matches the bin/ PWA icons so the logo and app icon stay identical.
static const char* LibretroLogoBitmap[RAYLIB_LIBRETRO_LOGO_ROWS] = {
    "00100000100",
    "00010001000",
    "00111111100",
    "01101110110",
    "11111111111",
    "10111111101",
    "10100000101",
    "00011011000",
};

Image GetLibretroLogoEx(int scale, Color foreground, Color background) {
    if (scale < 1) {
        scale = 1;
    }

    int width = RAYLIB_LIBRETRO_LOGO_COLS * scale;
    int height = RAYLIB_LIBRETRO_LOGO_ROWS * scale;
    Image image = GenImageColor(width, height, background);

    for (int y = 0; y < RAYLIB_LIBRETRO_LOGO_ROWS; y++) {
        for (int x = 0; x < RAYLIB_LIBRETRO_LOGO_COLS; x++) {
            if (LibretroLogoBitmap[y][x] == '1') {
                ImageDrawRectangle(&image, x * scale, y * scale, scale, scale, foreground);
            }
        }
    }

    return image;
}

Image GetLibretroLogo(void) {
    // Retro green invader on a transparent background, matching the app icon.
    Color invaderGreen = CLITERAL(Color){ 57, 211, 83, 255 };
    return GetLibretroLogoEx(16, invaderGreen, BLANK);
}

#if defined(__cplusplus)
}
#endif

#endif  // RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION_ONCE
#endif  // RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION
