// kernel/ui/window/window.c  (veya ui_window_draw nerede ise)

#include <ui/window/window.h>
#include <ui/window_chrome.h>
#include <ui/theme.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <stdint.h>
#include <stdbool.h>
#include <app/app.h>
#include <lib/string.h>

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static inline bool pt_in_rect(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && my >= y && mx < (x + w) && my < (y + h));
}

static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Darken: her kanalı amount kadar azalt (0..255)
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

// Title align helper: safe area içinde x hesapla
static int calc_title_x(const ui_theme_t* th, int safe_x, int safe_w, int text_px_w) {
    if (!th) return safe_x;
    if (safe_w < 0) safe_w = 0;
    if (text_px_w < 0) text_px_w = 0;

    switch (th->window_title_align) {
        default:
        case UI_ALIGN_LEFT:
            return safe_x;
        case UI_ALIGN_CENTER:
            return safe_x + (safe_w - text_px_w) / 2;
        case UI_ALIGN_RIGHT:
            return safe_x + (safe_w - text_px_w);
    }
}

// Basit text width hesabı (8px monospace varsayımı)
static int text_width8(const char* s) {
    if (!s) return 0;
    int w = 0;
    while (*s++) w += 8;
    return w;
}

// ------------------------------------------------------------
// Tabs drawing helpers (titlebar)
// ------------------------------------------------------------
typedef struct { int x, y, w, h; } chrome_rect_t;
static chrome_rect_t cr(int x, int y, int w, int h) { chrome_rect_t r = {x,y,w,h}; return r; }

static void draw_tab(int x, int y, int w, int h, const char* text, int active) {
    uint32_t bg = active ? 0x2A2A2A : 0x1C1C1C;
    uint32_t bd = active ? 0xFF6A00 : 0x404040;
    uint32_t fg = 0xFFFFFF;

    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, bd);

    int tx = x + 8;
    int ty = y + (h - 16) / 2; // 8x16 varsayımı
    gfx_draw_text_utf8(tx, ty, fg, text ? text : "");
}

static void draw_tab_add(int x, int y, int w, int h) {
    gfx_fill_rect(x, y, w, h, 0x1C1C1C);
    gfx_draw_rect(x, y, w, h, 0x404040);
    gfx_draw_text_utf8(x + 9, y + (h - 16) / 2, 0xFFFFFF, "+");
}

// titlebar içinde: text_x ile buton bloğu arası strip
static chrome_rect_t chrome_tabstrip_rect(const ui_window_t* win, const ui_chrome_layout_t* L) {
    int gap = 6;

    // min button pencerenin sağ yarısındaysa right layout varsay
    int right_layout = (L->btn_min_x > (win->x + win->w / 2)) ? 1 : 0;

    int left  = L->text_x;
    int right = win->x + win->w - 4;

    if (right_layout) {
        right = L->btn_min_x - gap;
    } else {
        left  = L->btn_min_x + L->btn_size + gap;
    }

    int w = right - left;
    if (w < 0) w = 0;

    return cr(left, win->y, w, L->title_h);
}

static void draw_header_right_chip(int x, int y, int w, int h, int r,
                                   const char* text, int active, int mx, int my) {
    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;

    // hover koymak istersen:
    if (mx >= x && mx < x+w && my >= y && my < y+h) bg = 0x3A3A3A;

    // shadow sadece sağa (alta değil)
    gfx_fill_round_rect4(x + 2, y, w, h, 0, r, 0, 0, shadow);
    gfx_fill_round_rect4(x, y, w, h, 0, r, 0, 0, bg);

    int len = (int)strlen(text ? text : "");
    int tw  = len * 8;
    gfx_draw_text_utf8(x + (w - tw)/2, y + (h - 16)/2, 0x00FFFFFF, text ? text : "");
}

static void draw_header_flat_chip(int x, int y, int w, int h,
                                  const char* text, int active, int mx, int my) {
    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;

    if (mx >= x && mx < x+w && my >= y && my < y+h) bg = 0x3A3A3A;

    gfx_fill_rect(x + 2, y, w, h, shadow);
    gfx_fill_rect(x, y, w, h, bg);

    int len = (int)strlen(text ? text : "");
    int tw  = len * 8;
    gfx_draw_text_utf8(x + (w - tw)/2, y + (h - 16)/2, 0x00FFFFFF, text ? text : "");
}

// ------------------------------------------------------------
// main draw
// ------------------------------------------------------------
void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my) {
    (void)is_active;

    if (!win) return;

    const ui_theme_t* th = ui_get_theme();
    if (!th) return;

    // --- Clamp theme values
    int title_h = clampi(th->window_title_h, 18, (win->h > 18 ? win->h : 18));
    int pad_l   = clampi(th->window_title_pad_l, 4, 64);
    int pad_r   = clampi(th->window_title_pad_r, 4, 64);

    // --- Base colors from theme
    const uint32_t col_win_bg     = (uint32_t)th->window_bg;
    const uint32_t col_title_bg   = (uint32_t)th->window_title_bg;
    const uint32_t col_title_text = (uint32_t)th->window_title_text;

    // ------------------------------------------------------------
    // ROUNDED WINDOW DRAW
    // ------------------------------------------------------------
    int r = 18; // 16-20 iyi
    if (r > win->w / 2) r = win->w / 2;
    if (r > win->h / 2) r = win->h / 2;

    // shadow: altta belli olsun (demo gibi)
    uint32_t shadow = 0x101010;
    gfx_fill_round_rect(win->x + 2, win->y + 6, win->w, win->h, r, shadow);

    // body: tüm köşeler yuvarlak
    gfx_fill_round_rect(win->x, win->y, win->w, win->h, r, col_win_bg);

    // header: sadece üst köşeler yuvarlak (alt düz)
    gfx_fill_round_rect4(win->x, win->y, win->w, title_h, r, r, 0, 0, col_title_bg);

    // ------------------------------------------------------------
    // Header chips (3'lü grup): [+] [-] [>]
    // ------------------------------------------------------------
    int chip_h = title_h;
    int chip_w = 44;
    int gap    = 0;

    int x_right = win->x + win->w - chip_w;
    int x_mid   = x_right - gap - chip_w;
    int x_left  = x_mid   - gap - chip_w;
    int y0      = win->y;

    draw_header_flat_chip(x_left,  y0, chip_w, chip_h, "+", false, mx, my);
    draw_header_flat_chip(x_mid,   y0, chip_w, chip_h, "-", false, mx, my);
    draw_header_right_chip(x_right,y0, chip_w, chip_h, 12, ">", false, mx, my);

    // ------------------------------------------------------------
    // Title safe area (chip’lere çarpmasın)
    // ------------------------------------------------------------
    int right_block = chip_w * 3; // 3 chip alanı
    int safe_x = win->x + pad_l;
    int safe_w = win->w - pad_l - pad_r - right_block;
    if (safe_w < 0) safe_w = 0;

    // ------------------------------------------------------------
    // Tabs (şimdilik opsiyonel)
    // ------------------------------------------------------------
    int drew_tabs = 0;

    // ⚠️ ui_chrome_layout() halen eski buton sistemine göre hesap yapıyorsa
    // strip yanlış çıkabilir. Sorun çıkarıyorsa burayı geçici kapat:
    /*
    app_t* app = (app_t*)win->user_data;
    if (app && app->v &&
        app->v->tabs_count && app->v->tabs_title &&
        app->v->tabs_active && app->v->tabs_set_active) {

        int n = app->v->tabs_count(app);
        if (n > 0) {
            ui_chrome_layout_t L = ui_chrome_layout(win);
            chrome_rect_t strip = chrome_tabstrip_rect(win, &L);

            if (strip.w > 30) {
                int tab_h = L.btn_size;
                if (tab_h < 14) tab_h = 14;

                int tab_w = 90;
                int tab_gap = 6;

                int max_tabs = (n < 3) ? n : 3;

                int yy = strip.y + (strip.h - tab_h) / 2;
                int xx = strip.x;

                for (int i = 0; i < max_tabs; i++) {
                    if (xx + tab_w > strip.x + strip.w) break;

                    const char* tt = app->v->tabs_title(app, i);
                    int active = (app->v->tabs_active(app) == i) ? 1 : 0;

                    draw_tab(xx, yy, tab_w, tab_h, tt, active);
                    xx += tab_w + tab_gap;
                }

                int add_w = 28;
                if (xx + add_w <= strip.x + strip.w) {
                    draw_tab_add(xx, yy, add_w, tab_h);
                }

                drew_tabs = 1;
            }
        }
    }
    */

    // ------------------------------------------------------------
    // Title text (tabs yoksa)
    // ------------------------------------------------------------
    if (!drew_tabs) {
        if (win->title && win->title[0]) {
            int text_px_w = text_width8(win->title);

            // Align: theme'den yararlan
            int tx = calc_title_x(th, safe_x, safe_w, text_px_w);

            const int font_h = 16;
            int ty = win->y + (title_h - font_h) / 2;
            if (ty < win->y) ty = win->y;

            gfx_draw_text_utf8(tx, ty, col_title_text, win->title);
        }
    }
}