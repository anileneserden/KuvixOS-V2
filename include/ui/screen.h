#pragma once
#include <stdint.h>

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
    uint32_t color;
    int visible;
    int used;
} ui_label_t;

typedef struct {
    uint32_t background_color;
    int loaded;

    ui_panel_t panel; /* ilk sürüm: sadece 1 panel */
    ui_label_t label; /* ilk sürüm: sadece 1 label */
} ui_screen_t;

int ui_screen_load(const char* path, ui_screen_t* out);
void ui_screen_render(const ui_screen_t* screen);
uint32_t ui_parse_color(const char* s);