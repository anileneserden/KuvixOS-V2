#include <ui/window_chrome.h>
#include <ui/window/window.h>

ui_chrome_layout_t ui_chrome_layout(const ui_window_t* win) {
    ui_chrome_layout_t L;
    L.title_h  = 24;
    L.btn_size = 16;
    L.pad      = 4;
    L.btn_y    = win->y + L.pad;

    L.btn_close_x = win->x + win->w - L.btn_size - L.pad;
    L.btn_max_x   = L.btn_close_x - L.btn_size - L.pad;
    L.btn_min_x   = L.btn_max_x   - L.btn_size - L.pad;

    L.icon_x = win->x + L.pad;
    if (win->icon != (void*)0) {
        L.text_x = L.icon_x + L.btn_size + L.pad;
    } else {
        L.text_x = L.icon_x + L.pad;
    }

    L.grip = 10;
    return L;
}