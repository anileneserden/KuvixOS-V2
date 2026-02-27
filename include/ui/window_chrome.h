// src/lib/ui/window_chrome.h
#pragma once
#include <stdint.h>
#include <ui/window/window.h>
#include <ui/wm/hittest.h>

typedef struct {
    int title_h;
    int btn_size;
    int pad;
    int btn_y;
    int btn_close_x;
    int btn_max_x;
    int btn_min_x;
    int grip;
    
    // Yeni eklenenler:
    int icon_x;    // İkonun çizileceği X
    int text_x;    // Başlık metninin başlayacağı X
} ui_chrome_layout_t;

ui_chrome_layout_t ui_chrome_layout(const ui_window_t* win);

// mouse hit test: mx,my -> hangi bölge?
wm_hittest_t ui_chrome_hittest(const ui_window_t* win, int mx, int my);

ui_rect_t ui_chrome_tabstrip_rect(const ui_window_t* win, const ui_chrome_layout_t* L);

ui_rect_t ui_chrome_tab_rect(const ui_rect_t strip, int i, int tab_w, int tab_h, int gap);

ui_rect_t ui_chrome_tab_add_rect(const ui_rect_t strip, int tab_count, int tab_w, int tab_h, int gap);