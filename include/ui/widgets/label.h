#pragma once
#include <stdint.h>

typedef struct {
    int x, y;
    const char* text;
    uint32_t color;
} ui_label_t;

void ui_label_init(ui_label_t* l, int x, int y, const char* text, uint32_t color);
void ui_label_set_text(ui_label_t* l, const char* text);
void ui_label_draw(const ui_label_t* l);
