// ui/window_chrome.c
#include <ui/window_chrome.h>
#include <ui/theme.h>

static int clampi(int v, int lo, int hi){
    if(v<lo) return lo;
    if(v>hi) return hi;
    return v;
}

ui_chrome_layout_t ui_chrome_layout(const ui_window_t* win) {
    ui_chrome_layout_t L = {0};

    // defaults
    L.title_h = 24;
    L.btn_w   = 44;     // dikdörtgen genişlik
    L.btn_h   = 24;     // title_h ile eşitlenecek
    L.btn_y   = 0;
    L.grip    = 10;
    L.pad     = 4;

    if (!win) return L;

    const ui_theme_t* th = ui_get_theme();

    int title_h = th ? th->window_title_h : 24;
    title_h = clampi(title_h, 18, (win->h > 18 ? win->h : 18));

    // chip yüksekliği titlebar kadar
    L.title_h = title_h;
    L.btn_h   = title_h;

    // Y: titlebar’ın en üstü
    L.btn_y = win->y;

    // GAP istersen burada koy (0 da olur)
    int gap = th ? th->window_btn_gap : 0;
    gap = clampi(gap, 0, 24);

    L.btn_close_x = win->x + win->w - L.btn_w;
    L.btn_max_x   = L.btn_close_x - L.btn_w;
    L.btn_min_x   = L.btn_max_x   - L.btn_w;

    // title/icon/text
    int pad_l = th ? th->window_title_pad_l : 8;
    pad_l = clampi(pad_l, 2, 64);

    L.icon_x = win->x + pad_l;
    L.text_x = L.icon_x;

    return L;
}