#include <ui/window/window.h>  // Önce temel tip
#include <ui/wm/hittest.h>     // Sonra hittest
#include <ui/window_chrome.h>

wm_hittest_t ui_chrome_hittest(const ui_window_t* win, int mx, int my) {
    if (!win) return HT_NONE;
    
    ui_chrome_layout_t L = ui_chrome_layout(win);

    if (mx < win->x || mx >= win->x + win->w || my < win->y || my >= win->y + win->h)
        return HT_NONE;

    if (mx >= L.btn_close_x && mx < L.btn_close_x + L.btn_size && my >= L.btn_y && my < L.btn_y + L.btn_size)
        return HT_BTN_CLOSE;
    
    if (mx >= L.btn_max_x && mx < L.btn_max_x + L.btn_size && my >= L.btn_y && my < L.btn_y + L.btn_size)
        return HT_BTN_MAX;
        
    if (mx >= L.btn_min_x && mx < L.btn_min_x + L.btn_size && my >= L.btn_y && my < L.btn_y + L.btn_size)
        return HT_BTN_MIN;

    if (my < win->y + L.title_h) return HT_TITLE;

    if (mx >= win->x + win->w - L.grip && my >= win->y + win->h - L.grip) 
        return HT_RESIZE_RIGHT_BOTTOM;

    return HT_CLIENT;
}