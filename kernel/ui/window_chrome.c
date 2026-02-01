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

wm_hittest_t ui_chrome_hittest(const ui_window_t* win, int mx, int my)
{
    if (!win) return HT_NONE;

    ui_chrome_layout_t L = ui_chrome_layout(win);

    // resize grip: sağ-alt 10x10
    if (in_rect(mx, my, win->x + win->w - L.grip, win->y + win->h - L.grip, L.grip, L.grip))
        return HT_GRIP_BR;

    // titlebar alanı
    if (in_rect(mx, my, win->x, win->y, win->w, L.title_h)) {
        if (in_rect(mx, my, L.btn_min_x, L.btn_y, L.btn_size, L.btn_size))   return HT_BTN_MIN;
        if (in_rect(mx, my, L.btn_max_x, L.btn_y, L.btn_size, L.btn_size))   return HT_BTN_MAX;
        if (in_rect(mx, my, L.btn_close_x, L.btn_y, L.btn_size, L.btn_size)) return HT_BTN_CLOSE;
        return HT_TITLE;
    }

    // geri kalan: client
    if (in_rect(mx, my, win->x, win->y, win->w, win->h))
        return HT_CLIENT;

    return HT_NONE;
}
