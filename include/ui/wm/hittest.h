#pragma once

#include <ui/window/window.h>

typedef enum {
    HT_NONE = 0,
    HT_TITLE,
    HT_BTN_CLOSE,
    HT_BTN_MIN,
    HT_BTN_MAX,
    HT_CLIENT,
    HT_GRIP_BR,
    HT_RESIZE_LEFT,
    HT_RESIZE_RIGHT,
    HT_RESIZE_TOP,
    HT_RESIZE_BOTTOM,
    HT_RESIZE_TOP_LEFT,
    HT_RESIZE_TOP_RIGHT,
    HT_RESIZE_BOTTOM_LEFT,
    HT_RESIZE_BOTTOM_RIGHT,
    HT_RESIZE_RIGHT_BOTTOM = 10 // Manuel atama çakışma yapmasın diye kontrol et
} wm_hittest_t;

// Bu satırı ekle (Eğer yoksa):
wm_hittest_t ui_chrome_hittest(const ui_window_t* win, int mx, int my);