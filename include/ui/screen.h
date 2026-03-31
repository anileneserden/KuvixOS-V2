#pragma once

#include <stdint.h>
#include <ui/desktop_icons.h>

typedef struct {
    char id[64];
    int x;
    int y;
    int width;
    int height;
    uint32_t background_color;
    int visible;
    int used;
} ui_panel_t;

typedef struct {
    char id[64];
    int x;
    int y;
    char text[128];
    char bind[64];
    char format[64];
    uint32_t color;
    int visible;
    int used;
} ui_label_t;

typedef struct {
    char id[64];
    int used;
    int visible;
    desktop_icons_style_t style;
} ui_desktop_icons_t;

typedef struct {
    int used;
    int visible;

    char id[32];
    char title[64];

    int x;
    int y;
    int width;
    int height;

    int radius;
    int border_thickness;
    int titlebar_height;

    uint32_t background_color;
    uint32_t titlebar_color;
    uint32_t title_color;
} ui_screen_window_t;

typedef struct {
    uint32_t background_color;
    int loaded;

    ui_panel_t panel;
    ui_label_t label;
    ui_desktop_icons_t desktop_icons;
    ui_screen_window_t window;
} ui_screen_t;

int ui_screen_load(const char* path, ui_screen_t* out);
void ui_screen_render(const ui_screen_t* screen);
uint32_t ui_parse_color(const char* s);