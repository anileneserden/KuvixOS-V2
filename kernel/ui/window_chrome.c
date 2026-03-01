// kernel/ui/window_chrome.c
// Theme destekli, clamp'li layout

#include <ui/window_chrome.h>
#include <ui/window/window.h>
#include <ui/theme.h>

// küçük clamp helper
static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

ui_chrome_layout_t ui_chrome_layout(const ui_window_t* win) {
    ui_chrome_layout_t L;

    // default güvenli başlangıç (win NULL olsa da)
    L.title_h     = 24;
    L.btn_size    = 16;
    L.pad         = 4;
    L.btn_y       = 4;
    L.btn_close_x = 0;
    L.btn_max_x   = 0;
    L.btn_min_x   = 0;
    L.icon_x      = 4;
    L.text_x      = 8;
    L.grip        = 10;

    if (!win) return L;

    const ui_theme_t* th = ui_get_theme();

    // --- Theme values (fallback + clamp) ---
    int border_px = (th ? th->window_border_px : 1);
    if (border_px <= 0) border_px = 1;
    border_px = clampi(border_px, 1, 16);

    int title_h = (th ? th->window_title_h : 24);
    title_h = clampi(title_h, 18, (win->h > 18 ? win->h : 18));

    int btn_size = (th ? th->window_btn_size : 16);
    btn_size = clampi(btn_size, 10, title_h - 4);

    int btn_gap = (th ? th->window_btn_gap : 4);
    btn_gap = clampi(btn_gap, 2, 24);

    int btn_margin = (th ? th->window_btn_margin : 6);
    btn_margin = clampi(btn_margin, 2, 64);

    int pad_l = (th ? th->window_title_pad_l : 8);
    int pad_r = (th ? th->window_title_pad_r : 8);
    pad_l = clampi(pad_l, 2, 64);
    pad_r = clampi(pad_r, 2, 64);

    // genel chrome padding
    int pad = (th ? th->window_btn_margin : 4);
    pad = clampi(pad, 2, 64);

    // --- Apply ---
    L.title_h  = title_h;
    L.btn_size = btn_size;
    L.pad      = pad;

    // Butonların dikey hizası: titlebar ortası
    L.btn_y = win->y + (title_h - btn_size) / 2;

    // --- Button layout: left/right ---
    int right = 1;
    if (th && th->window_btn_layout == UI_BTN_LAYOUT_LEFT) right = 0;

    if (right) {
        int x = win->x + win->w - border_px - btn_margin - btn_size;

        L.btn_close_x = x;
        x -= (btn_size + btn_gap);
        L.btn_max_x = x;
        x -= (btn_size + btn_gap);
        L.btn_min_x = x;

        // Soldan icon + text
        L.icon_x = win->x + border_px + pad_l;

        if (win->icon != (void*)0) {
            L.text_x = L.icon_x + btn_size + pad_l;
        } else {
            L.text_x = L.icon_x;
        }

        (void)pad_r;
    } else {
        int x = win->x + border_px + btn_margin;

        L.btn_close_x = x;
        x += (btn_size + btn_gap);
        L.btn_max_x = x;
        x += (btn_size + btn_gap);
        L.btn_min_x = x;

        // soldaki butonların sağına text'i it
        L.icon_x = win->x + border_px + pad_l;

        int left_block_end = L.btn_min_x + btn_size;
        int base_text_x = left_block_end + pad_l;

        if (win->icon != (void*)0) {
            L.text_x = base_text_x + btn_size + pad_l;
        } else {
            L.text_x = base_text_x;
        }
    }

    // Grip (resize) alanı
    L.grip = 10;

    return L;
}