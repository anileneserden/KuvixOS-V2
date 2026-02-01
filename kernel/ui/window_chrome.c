// src/lib/ui/window/window_chrome.c
#include <ui/window_chrome.h>

static int in_rect(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

ui_chrome_layout_t ui_chrome_layout(const ui_window_t* win)
{
    ui_chrome_layout_t L;
    L.title_h  = 24;
    L.btn_size = 16;
    L.pad      = 4;
    L.btn_y    = win->y + L.pad;

    int x = win->x;
    int ww = win->w;

    L.btn_close_x = x + ww - L.btn_size - L.pad;
    L.btn_max_x   = L.btn_close_x - L.btn_size - L.pad;
    L.btn_min_x   = L.btn_max_x   - L.btn_size - L.pad;

    L.grip = 10;
    return L;
}
