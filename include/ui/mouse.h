// src/lib/ui/moue/mouse.h
#pragma once
#include <stdint.h>

typedef struct {
    int x;
    int y;
    int visible;

    uint32_t screen_w;
    uint32_t screen_h;

    int buttons;      // bit0: L, bit1: R, bit2: M
    int prev_buttons; // bir önceki değer
} mouse_t;

extern mouse_t g_mouse;

void mouse_init(uint32_t screen_w, uint32_t screen_h);
void mouse_set_position(int x, int y);
void mouse_move(int dx, int dy);
void mouse_draw(void);
void mouse_show(void);
void mouse_hide(void);