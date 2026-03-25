#pragma once
#include <stdint.h>

typedef struct {
    uint32_t background_color;
    int loaded;
} ui_screen_t;

// JSON'dan screen yükler
int ui_screen_load(const char* path, ui_screen_t* out);

// framebuffer'a render eder
void ui_screen_render(const ui_screen_t* screen);

// "#RRGGBB" -> uint32_t
uint32_t ui_parse_color(const char* s);