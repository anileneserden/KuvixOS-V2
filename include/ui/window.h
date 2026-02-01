// src/lib/ui/window/window.h
#pragma once
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

#define UI_TITLE_H 24

static inline ui_rect_t ui_window_client_rect(const ui_window_t* win) {
    ui_rect_t r;
    r.x = win->x + 8;
    r.y = win->y + UI_TITLE_H + 8;
    r.w = win->w - 16;
    r.h = win->h - (UI_TITLE_H + 16);
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

void ui_window_draw(const ui_window_t* w, int is_active, int mx, int my);

// eski wrapper (kolay demo için)
void ui_draw_window(int x, int y, int w, int h, const char* title);

// hit test için yardımcılar (WM'de çok işine yarar)
static inline int ui_window_contains(const ui_window_t* w, int px, int py) {
    return (px >= w->x && px < (w->x + w->w) && py >= w->y && py < (w->y + w->h));
}

// titlebar varsayımı: 24px
static inline int ui_window_in_titlebar(const ui_window_t* w, int px, int py) {
    return ui_window_contains(w, px, py) && (py >= w->y && py < (w->y + 24));
}
