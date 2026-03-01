#pragma once
#include <stdint.h>

typedef struct {
    int x, y;
    uint32_t color;
    const char* text;   // UTF-8
} ui_label_t;

void ui_label_draw(const ui_label_t* l);