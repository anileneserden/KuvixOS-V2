// include/ui/controls/panel2.h
#pragma once
#include <stdint.h>
#include <ui/controls/control.h>

typedef struct {
    ui_control_t base;
    uint32_t bg_color;   // 0xRRGGBB
    int draw_border;
    uint32_t border_color;
} ui_panel2_t;

void ui_panel2_init(ui_panel2_t* p,
                    int id,
                    ui_point_t loc,
                    ui_size_t size,
                    uint32_t bg_color);

void ui_panel2_set_border(ui_panel2_t* p, int enable, uint32_t border_color);
