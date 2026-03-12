// ui/window/window.c  (ui_window_draw içi düzeltildi)

#include <ui/window/window.h>
#include <ui/window_chrome.h>
#include <ui/theme.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <stdint.h>
#include <stdbool.h>
#include <app/app.h>
#include <lib/string.h>

#include <ui/bitmaps/icons/icon_close_16.h>
#include <ui/bitmaps/icons/icon_max_16.h>
#include <ui/bitmaps/icons/icon_min_16.h>

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline uint32_t darken_rgb(uint32_t c, int amount) {
    amount = clampi(amount, 0, 255);

    int r = (c >> 16) & 0xFF;
    int g = (c >> 8)  & 0xFF;
    int b = (c)       & 0xFF;

    r -= amount; if (r < 0) r = 0;
    g -= amount; if (g < 0) g = 0;
    b -= amount; if (b < 0) b = 0;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static int calc_title_x(const ui_theme_t* th, int safe_x, int safe_w, int text_px_w) {
    if (!th) return safe_x;
    if (safe_w < 0) safe_w = 0;
    if (text_px_w < 0) text_px_w = 0;

    switch (th->window_title_align) {
        default:
        case UI_ALIGN_LEFT:   return safe_x;
        case UI_ALIGN_CENTER: return safe_x + (safe_w - text_px_w) / 2;
        case UI_ALIGN_RIGHT:  return safe_x + (safe_w - text_px_w);
    }
}

static int text_width8(const char* s) {
    if (!s) return 0;
    int w = 0;
    while (*s++) w += 8;
    return w;
}

static void draw_icon_center_key(int bx, int by, int bw, int bh,
                                 int iw, int ih, const uint32_t* argb, uint32_t key)
{
    if (!argb) return;

    int ix = bx + (bw - iw) / 2;
    int iy = by + (bh - ih) / 2;

    gfx_blit_argb_key(ix, iy, iw, ih, argb, key);
}

// ------------------------------------------------------------
// Header chip draw
// ------------------------------------------------------------
static void draw_header_right_chip(int x, int y, int w, int h, int r,
                                   const char* text, int active, int mx, int my) {
    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;

    // hover
    if (mx >= x && mx < x+w && my >= y && my < y+h) bg = 0x3A3A3A;

    gfx_fill_round_rect4(x + 2, y, w, h, 0, r, 0, 0, shadow);
    gfx_fill_round_rect4(x, y, w, h, 0, r, 0, 0, bg);

    if (!text) text = "";
    int len = (int)strlen(text);
    int tw  = len * 8;
    gfx_draw_text_utf8(x + (w - tw)/2, y + (h - 16)/2, 0x00FFFFFF, text);
}

static void draw_header_right_chip_transparent(
    int x, int y, int w, int h, int r,
    uint32_t title_bg, int mx, int my
) {
    if (mx >= x && mx < x+w && my >= y && my < y+h) {
        gfx_fill_round_rect4(x, y, w, h, 0, r, 0, 0, darken_rgb(title_bg, 18));
    }

    gfx_fill_rect(x, y, 2, h, 0x2A2A2A);
}

static void draw_header_flat_chip(int x, int y, int w, int h,
                                  const char* text, int active, int mx, int my) {
    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;

    if (mx >= x && mx < x+w && my >= y && my < y+h) bg = 0x3A3A3A;

    gfx_fill_rect(x + 2, y, w, h, shadow);
    gfx_fill_rect(x, y, w, h, bg);

    if (!text) text = "";
    int len = (int)strlen(text);
    int tw  = len * 8;
    gfx_draw_text_utf8(x + (w - tw)/2, y + (h - 16)/2, 0x00FFFFFF, text);
}

static void draw_header_flat_chip_transparent(
    int x, int y, int w, int h,
    uint32_t title_bg, int mx, int my
) {
    if (mx >= x && mx < x+w && my >= y && my < y+h) {
        gfx_fill_rect(x, y, w, h, darken_rgb(title_bg, 18));
    }

    gfx_fill_rect(x, y, 2, h, 0x2A2A2A);
}

// ------------------------------------------------------------
// main draw
// ------------------------------------------------------------
void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my) {
    (void)is_active;
    if (!win) return;

    const ui_theme_t* th = ui_get_theme();
    if (!th) return;
    
    ui_chrome_layout_t L = ui_chrome_layout(win);
    int title_h = L.title_h;
    int pad_l   = clampi(th->window_title_pad_l, 4, 64);
    int pad_r   = clampi(th->window_title_pad_r, 4, 64);

    const uint32_t col_win_bg     = (uint32_t)th->window_bg;
    const uint32_t col_title_bg   = (uint32_t)th->window_title_bg;
    const uint32_t col_title_text = (uint32_t)th->window_title_text;

    // ------------------------------------------------------------
    // Rounded window
    // ------------------------------------------------------------
    int rr = 18;
    if (rr > win->w / 2) rr = win->w / 2;
    if (rr > win->h / 2) rr = win->h / 2;

    uint32_t shadow = 0x101010;
    gfx_fill_round_rect(win->x + 2, win->y + 6, win->w, win->h, rr, shadow);
    gfx_fill_round_rect(win->x, win->y, win->w, win->h, rr, col_win_bg);
    gfx_fill_round_rect4(win->x, win->y, win->w, title_h, rr, rr, 0, 0, col_title_bg);

    // ------------------------------------------------------------
    // Header chips: ✅ L’den çiz (dikdörtgen)
    // ------------------------------------------------------------
    int chip_r = 12;

    uint32_t chip_bg   = darken_rgb(col_title_bg, 35);
    uint32_t chip_bg_h = darken_rgb(col_title_bg, 55);

    // MIN
    gfx_fill_rect(L.btn_min_x, L.btn_y, L.btn_w, L.btn_h,
                (mx>=L.btn_min_x && mx<L.btn_min_x+L.btn_w && my>=L.btn_y && my<L.btn_y+L.btn_h) ? chip_bg_h : chip_bg);

    // MAX
    gfx_fill_rect(L.btn_max_x, L.btn_y, L.btn_w, L.btn_h,
                (mx>=L.btn_max_x && mx<L.btn_max_x+L.btn_w && my>=L.btn_y && my<L.btn_y+L.btn_h) ? chip_bg_h : chip_bg);

    // CLOSE (sağ köşe yuvarlak kalsın)
    int r = 12;
    uint32_t cbg = (mx>=L.btn_close_x && mx<L.btn_close_x+L.btn_w && my>=L.btn_y && my<L.btn_y+L.btn_h) ? chip_bg_h : chip_bg;
    gfx_fill_round_rect4(L.btn_close_x, L.btn_y, L.btn_w, L.btn_h, 0, r, 0, 0, cbg);

    // Transparent key: 0x00000000 (senin palette[0] bu)
    const uint32_t KEY = 0x00000000u;

    // min/max ikonların da 16x16 ise:
    draw_icon_center_key(L.btn_min_x,   L.btn_y, L.btn_w, L.btn_h,
                        16, 16, g_icon_min_16, KEY);

    draw_icon_center_key(L.btn_max_x,   L.btn_y, L.btn_w, L.btn_h,
                        16, 16, g_icon_max_16, KEY);

    draw_icon_center_key(L.btn_close_x, L.btn_y, L.btn_w, L.btn_h,
                        16, 16, g_icon_close_16, KEY);

    // ------------------------------------------------------------
    // Title safe area (buton bloğuna çarpmasın)
    // ------------------------------------------------------------
    int block_left  = L.btn_min_x;
    int block_right = L.btn_close_x + L.btn_w;

    int safe_x = win->x + pad_l;
    int safe_w = block_left - safe_x;
    if (safe_w < 0) safe_w = 0;

    // butonlar sağdaysa: sağ blok title'ı daraltsın
    int right_layout = (block_left > (win->x + win->w/2)) ? 1 : 0;

    if (right_layout) {
        safe_w = block_left - safe_x;
    } else {
        safe_x = block_right + pad_l;
        safe_w = (win->x + win->w - pad_r) - safe_x;
    }

    if (safe_w < 0) safe_w = 0;

    // ------------------------------------------------------------
    // Title text
    // ------------------------------------------------------------
    if (win->title && win->title[0]) {
        int text_px_w = text_width8(win->title);
        int tx = calc_title_x(th, safe_x, safe_w, text_px_w);

        const int font_h = 16;
        int ty = win->y + (title_h - font_h) / 2;
        if (ty < win->y) ty = win->y;

        gfx_draw_text_utf8(tx, ty, col_title_text, win->title);
    }
}