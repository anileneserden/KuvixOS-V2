#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

typedef enum {
    WIN_NORMAL = 0,
    WIN_MINIMIZED,
    WIN_MAXIMIZED
} win_state_t;

typedef struct {
    int x, y, w, h;
    int prev_x, prev_y, prev_w, prev_h;
    win_state_t state;
    const char* title;
} ui_window_t;

typedef struct {
    int x, y, w, h;
} ui_rect_t;

// wm.c içindeki çağrıya göre 4 parametreli olmalı
void ui_window_draw(const ui_window_t* win, int active, int mx, int my);

#endif