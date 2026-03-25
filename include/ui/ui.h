#ifndef KERNEL_UI_UI_H
#define KERNEL_UI_UI_H

#include <stdint.h>

typedef struct {
    uint32_t background_color;
} ui_screen_t;

int ui_load_screen(const char* path, ui_screen_t* out_screen);
uint32_t ui_parse_color(const char* s);
void ui_render_screen(const ui_screen_t* screen, uint32_t* fb, int width, int height);

#endif