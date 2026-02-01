// src/lib/ui/window/window_chrome.h
#pragma once
#include <stdint.h>
#include "window.h"
#include <ui/wm/hittest.h>

typedef struct {
    int title_h;
    int btn_size;
    int pad;
    int btn_y;

    int btn_min_x;
    int btn_max_x;
    int btn_close_x;

    int grip; // square size
} ui_chrome_layout_t;

ui_chrome_layout_t ui_chrome_layout(const ui_window_t* win);

// mouse hit test: mx,my -> hangi bölge?
wm_hittest_t ui_chrome_hittest(const ui_window_t* win, int mx, int my);
