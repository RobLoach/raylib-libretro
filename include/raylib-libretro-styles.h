/**********************************************************************************************
*
*   raylib-libretro-styles.h - Menu theme/style definitions for raylib-libretro.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_STYLES_H
#define RAYLIB_LIBRETRO_STYLES_H

#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"

typedef enum LibretroMenuStyle {
    LIBRETRO_MENU_STYLE_CATPPUCCIN_MOCHA,
    LIBRETRO_MENU_STYLE_CATPPUCCIN_LATTE,
    LIBRETRO_MENU_STYLE_CATPPUCCIN_FRAPPE,
    LIBRETRO_MENU_STYLE_CATPPUCCIN_MACCHIATO,
    LIBRETRO_MENU_STYLE_DRACULA,
    LIBRETRO_MENU_STYLE_DARK,
    LIBRETRO_MENU_STYLE_COUNT,
} LibretroMenuStyle;

/**
 * The style names, seperated by a |
 */
#define RAYLIB_LIBRETRO_STYLES_NAMES "Mocha|Latte|Frappe|Macchiato|Dracula|Dark"

/**
 * Sets the menu style.
 */
static void SetLibretroMenuStyle(LibretroMenuStyle style);

#endif // RAYLIB_LIBRETRO_STYLES_H

#ifdef RAYLIB_LIBRETRO_STYLES_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_STYLES_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_STYLES_IMPLEMENTATION_ONCE

static void SetLibretroMenuStyle(LibretroMenuStyle style) {
    if (!IsLibretroMenuReady()) {
        return;
    }
    struct nk_color table[NK_COLOR_COUNT];
    struct nk_context* ctx = menu.ctx;

    // Reset the styles to default first.
    nk_style_default(menu.ctx);

    switch (style) {
        case LIBRETRO_MENU_STYLE_DRACULA: {
            struct nk_color background = nk_rgba(40, 42, 54, 235);
            struct nk_color currentline = nk_rgba(68, 71, 90, 255);
            struct nk_color foreground = nk_rgba(248, 248, 242, 255);
            struct nk_color comment = nk_rgba(98, 114, 164, 255);
            /* struct nk_color cyan = nk_rgba(139, 233, 253, 255); */
            /* struct nk_color green = nk_rgba(80, 250, 123, 255); */
            /* struct nk_color orange = nk_rgba(255, 184, 108, 255); */
            struct nk_color pink = nk_rgba(255, 121, 198, 255);
            struct nk_color purple = nk_rgba(189, 147, 249, 255);
            /* struct nk_color red = nk_rgba(255, 85, 85, 255); */
            /* struct nk_color yellow = nk_rgba(241, 250, 140, 255); */
            table[NK_COLOR_TEXT] = foreground;
            table[NK_COLOR_WINDOW] = background;
            table[NK_COLOR_HEADER] = currentline;
            table[NK_COLOR_BORDER] = currentline;
            table[NK_COLOR_BUTTON] = currentline;
            table[NK_COLOR_BUTTON_HOVER] = comment;
            table[NK_COLOR_BUTTON_ACTIVE] = purple;
            table[NK_COLOR_TOGGLE] = currentline;
            table[NK_COLOR_TOGGLE_HOVER] = comment;
            table[NK_COLOR_TOGGLE_CURSOR] = pink;
            table[NK_COLOR_SELECT] = currentline;
            table[NK_COLOR_SELECT_ACTIVE] = comment;
            table[NK_COLOR_SLIDER] = background;
            table[NK_COLOR_SLIDER_CURSOR] = currentline;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = comment;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = comment;
            table[NK_COLOR_PROPERTY] = currentline;
            table[NK_COLOR_EDIT] = currentline;
            table[NK_COLOR_EDIT_CURSOR] = foreground;
            table[NK_COLOR_COMBO] = currentline;
            table[NK_COLOR_CHART] = currentline;
            table[NK_COLOR_CHART_COLOR] = comment;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = purple;
            table[NK_COLOR_SCROLLBAR] = background;
            table[NK_COLOR_SCROLLBAR_CURSOR] = currentline;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = comment;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = purple;
            table[NK_COLOR_TAB_HEADER] = currentline;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = table[NK_COLOR_SLIDER_CURSOR];
            table[NK_COLOR_KNOB_CURSOR_HOVER] = table[NK_COLOR_SLIDER_CURSOR_HOVER];
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = table[NK_COLOR_SLIDER_CURSOR_ACTIVE];
            nk_style_from_table(menu.ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_LATTE: {
            /*struct nk_color rosewater = nk_rgba(220, 138, 120, 255);*/
            /*struct nk_color flamingo = nk_rgba(221, 120, 120, 255);*/
            struct nk_color pink = nk_rgba(234, 118, 203, 255);
            struct nk_color mauve = nk_rgba(136, 57, 239, 255);
            /*struct nk_color red = nk_rgba(210, 15, 57, 255);*/
            /*struct nk_color maroon = nk_rgba(230, 69, 83, 255);*/
            /*struct nk_color peach = nk_rgba(254, 100, 11, 255);*/
            struct nk_color yellow = nk_rgba(223, 142, 29, 255);
            /*struct nk_color green = nk_rgba(64, 160, 43, 255);*/
            struct nk_color teal = nk_rgba(23, 146, 153, 255);
            /*struct nk_color sky = nk_rgba(4, 165, 229, 255);*/
            /*struct nk_color sapphire = nk_rgba(32, 159, 181, 255);*/
            /*struct nk_color blue = nk_rgba(30, 102, 245, 255);*/
            /*struct nk_color lavender = nk_rgba(114, 135, 253, 255);*/
            struct nk_color text = nk_rgba(76, 79, 105, 255);
            /*struct nk_color subtext1 = nk_rgba(92, 95, 119, 255);*/
            /*struct nk_color subtext0 = nk_rgba(108, 111, 133, 255);*/
            struct nk_color overlay2 = nk_rgba(124, 127, 147, 55);
            /*struct nk_color overlay1 = nk_rgba(140, 143, 161, 255);*/
            struct nk_color overlay0 = nk_rgba(156, 160, 176, 255);
            struct nk_color surface2 = nk_rgba(172, 176, 190, 255);
            struct nk_color surface1 = nk_rgba(188, 192, 204, 255);
            struct nk_color surface0 = nk_rgba(204, 208, 218, 255);
            struct nk_color base = nk_rgba(239, 241, 245, 235);
            struct nk_color mantle = nk_rgba(230, 233, 239, 255);
            /*struct nk_color crust = nk_rgba(220, 224, 232, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay2;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = yellow;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = teal;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = teal;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = teal;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = mauve;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = teal;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = mauve;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = mauve;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = mauve;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_FRAPPE: {
            /*struct nk_color rosewater = nk_rgba(242, 213, 207, 255);*/
            /*struct nk_color flamingo = nk_rgba(238, 190, 190, 255);*/
            struct nk_color pink = nk_rgba(244, 184, 228, 255);
            /*struct nk_color mauve = nk_rgba(202, 158, 230, 255);*/
            /*struct nk_color red = nk_rgba(231, 130, 132, 255);*/
            /*struct nk_color maroon = nk_rgba(234, 153, 156, 255);*/
            /*struct nk_color peach = nk_rgba(239, 159, 118, 255);*/
            /*struct nk_color yellow = nk_rgba(229, 200, 144, 255);*/
            struct nk_color green = nk_rgba(166, 209, 137, 255);
            /*struct nk_color teal = nk_rgba(129, 200, 190, 255);*/
            /*struct nk_color sky = nk_rgba(153, 209, 219, 255);*/
            /*struct nk_color sapphire = nk_rgba(133, 193, 220, 255);*/
            /*struct nk_color blue = nk_rgba(140, 170, 238, 255);*/
            struct nk_color lavender = nk_rgba(186, 187, 241, 255);
            struct nk_color text = nk_rgba(198, 208, 245, 255);
            /*struct nk_color subtext1 = nk_rgba(181, 191, 226, 255);*/
            /*struct nk_color subtext0 = nk_rgba(165, 173, 206, 255);*/
            struct nk_color overlay2 = nk_rgba(148, 156, 187, 255);
            struct nk_color overlay1 = nk_rgba(131, 139, 167, 255);
            struct nk_color overlay0 = nk_rgba(115, 121, 148, 255);
            struct nk_color surface2 = nk_rgba(98, 104, 128, 255);
            struct nk_color surface1 = nk_rgba(81, 87, 109, 255);
            struct nk_color surface0 = nk_rgba(65, 69, 89, 255);
            struct nk_color base = nk_rgba(48, 52, 70, 235);
            struct nk_color mantle = nk_rgba(41, 44, 60, 255);
            /*struct nk_color crust = nk_rgba(35, 38, 52, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay1;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = pink;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = green;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = green;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = green;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = pink;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = lavender;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = pink;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = lavender;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = lavender;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_MACCHIATO: {
            /*struct nk_color rosewater = nk_rgba(244, 219, 214, 255);*/
            /*struct nk_color flamingo = nk_rgba(240, 198, 198, 255);*/
            struct nk_color pink = nk_rgba(245, 189, 230, 255);
            /*struct nk_color mauve = nk_rgba(198, 160, 246, 255);*/
            /*struct nk_color red = nk_rgba(237, 135, 150, 255);*/
            /*struct nk_color maroon = nk_rgba(238, 153, 160, 255);*/
            /*struct nk_color peach = nk_rgba(245, 169, 127, 255);*/
            struct nk_color yellow = nk_rgba(238, 212, 159, 255);
            struct nk_color green = nk_rgba(166, 218, 149, 255);
            /*struct nk_color teal = nk_rgba(139, 213, 202, 255);*/
            /*struct nk_color sky = nk_rgba(145, 215, 227, 255);*/
            /*struct nk_color sapphire = nk_rgba(125, 196, 228, 255);*/
            /*struct nk_color blue = nk_rgba(138, 173, 244, 255);*/
            struct nk_color lavender = nk_rgba(183, 189, 248, 255);
            struct nk_color text = nk_rgba(202, 211, 245, 255);
            /*struct nk_color subtext1 = nk_rgba(184, 192, 224, 255);*/
            /*struct nk_color subtext0 = nk_rgba(165, 173, 203, 255);*/
            struct nk_color overlay2 = nk_rgba(147, 154, 183, 255);
            struct nk_color overlay1 = nk_rgba(128, 135, 162, 255);
            struct nk_color overlay0 = nk_rgba(110, 115, 141, 255);
            struct nk_color surface2 = nk_rgba(91, 96, 120, 255);
            struct nk_color surface1 = nk_rgba(73, 77, 100, 255);
            struct nk_color surface0 = nk_rgba(54, 58, 79, 255);
            struct nk_color base = nk_rgba(36, 39, 58, 235);
            struct nk_color mantle = nk_rgba(30, 32, 48, 255);
            /*struct nk_color crust = nk_rgba(24, 25, 38, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay1;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = yellow;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = green;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = green;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = green;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = pink;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = lavender;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = yellow;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = lavender;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = lavender;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_MOCHA: {
            /*struct nk_color rosewater = nk_rgba(245, 224, 220, 255);*/
            /*struct nk_color flamingo = nk_rgba(242, 205, 205, 255);*/
            struct nk_color pink = nk_rgba(245, 194, 231, 255);
            /*struct nk_color mauve = nk_rgba(203, 166, 247, 255);*/
            /*struct nk_color red = nk_rgba(243, 139, 168, 255);*/
            /*struct nk_color maroon = nk_rgba(235, 160, 172, 255);*/
            /*struct nk_color peach = nk_rgba(250, 179, 135, 255);*/
            /*struct nk_color yellow = nk_rgba(249, 226, 175, 255);*/
            struct nk_color green = nk_rgba(166, 227, 161, 255);
            /*struct nk_color teal = nk_rgba(148, 226, 213, 255);*/
            /*struct nk_color sky = nk_rgba(137, 220, 235, 255);*/
            /*struct nk_color sapphire = nk_rgba(116, 199, 236, 255);*/
            /*struct nk_color blue = nk_rgba(137, 180, 250, 255);*/
            struct nk_color lavender = nk_rgba(180, 190, 254, 255);
            struct nk_color text = nk_rgba(205, 214, 244, 255);
            /*struct nk_color subtext1 = nk_rgba(186, 194, 222, 255);*/
            /*struct nk_color subtext0 = nk_rgba(166, 173, 200, 255);*/
            struct nk_color overlay2 = nk_rgba(147, 153, 178, 255);
            struct nk_color overlay1 = nk_rgba(127, 132, 156, 255);
            struct nk_color overlay0 = nk_rgba(108, 112, 134, 255);
            struct nk_color surface2 = nk_rgba(88, 91, 112, 255);
            struct nk_color surface1 = nk_rgba(69, 71, 90, 255);
            struct nk_color surface0 = nk_rgba(49, 50, 68, 255);
            struct nk_color base = nk_rgba(30, 30, 46, 235);
            struct nk_color mantle = nk_rgba(24, 24, 37, 255);
            /*struct nk_color crust = nk_rgba(17, 17, 27, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay1;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = lavender;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = green;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = green;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = green;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = lavender;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = lavender;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = pink;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = lavender;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = pink;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_DARK:
        default: {
            // Nothing
            break;
        }
    }
}

#endif // RAYLIB_LIBRETRO_STYLES_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_STYLES_IMPLEMENTATION
