#pragma once
#include <ui/controls/control.h>
#include <stdint.h>

typedef struct {
    ui_control_t base;
    uint32_t color;
    const char* text; // UTF-8
} ui_label2_t;

void ui_label2_init(ui_label2_t* l, int id, ui_point_t loc, uint32_t color, const char* text);
void ui_label2_set_text(ui_label2_t* l, const char* text);
