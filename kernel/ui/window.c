// kernel/ui/window/window.c  (veya ui_window_draw nerede ise)

#include <ui/window/window.h>
#include <ui/window_chrome.h>
#include <ui/theme.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <stdint.h>
#include <stdbool.h>
#include <app/app.h>

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

// ------------------------------------------------------------
// main draw
// ------------------------------------------------------------
void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my) {
    (void)is_active;

    if (!win) return;

    const ui_theme_t* th = ui_get_theme();
    if (!th) return;

    // --- Clamp theme values (0 gelirse UI bozulmasın)
    int border  = clampi(th->window_border_px, 1, 16);
    int title_h = clampi(th->window_title_h, 18, (win->h > 18 ? win->h : 18));

    int btn = clampi(th->window_btn_size, 10, title_h - 4);
    int gap = clampi(th->window_btn_gap, 2, 24);
    int mar = clampi(th->window_btn_margin, 2, 64);

    int pad_l = clampi(th->window_title_pad_l, 4, 64);
    int pad_r = clampi(th->window_title_pad_r, 4, 64);

    int btn_pad_l = clampi(th->window_btn_pad_left, 0, 128);
    int btn_pad_r = clampi(th->window_btn_pad_right, 0, 128);

    int hover_dark = clampi(th->window_btn_hover_dark, 0, 96);
    int press_dark = clampi(th->window_btn_press_dark, 0, 128);

    // --- Base colors from theme
    const uint32_t col_win_bg     = (uint32_t)th->window_bg;
    const uint32_t col_border     = (uint32_t)th->window_border;
    const uint32_t col_title_bg   = (uint32_t)th->window_title_bg;
    const uint32_t col_title_text = (uint32_t)th->window_title_text;

    // --- Window body
    fb_draw_rect(win->x, win->y, win->w, win->h, col_win_bg);

    // Border: border_px kadar outline
    for (int i = 0; i < border; i++) {
        int w = win->w - 2 * i;
        int h = win->h - 2 * i;
        if (w <= 0 || h <= 0) break;
        fb_draw_rect_outline(win->x + i, win->y + i, w, h, col_border);
    }

    // --- Title bar
    fb_draw_rect(win->x, win->y, win->w, title_h, col_title_bg);

    // --- Buttons layout (3 button)
    const int btn_total_w = (btn * 3) + (gap * 2);

    int by = win->y + (title_h - btn) / 2;
    if (by < win->y) by = win->y;

    bool btn_right = (th->window_btn_layout == UI_BTN_LAYOUT_RIGHT);

    int bx0 = btn_right
        ? (win->x + win->w - mar - btn)
        : (win->x + mar);

    // --- Validate order (bozuksa default: close,max,min)
    uint8_t o0 = th->window_btn_order[0];
    uint8_t o1 = th->window_btn_order[1];
    uint8_t o2 = th->window_btn_order[2];

    bool bad = (o0 > 2 || o1 > 2 || o2 > 2) || (o0 == o1) || (o0 == o2) || (o1 == o2);
    uint8_t order[3];
    if (bad) { order[0] = 0; order[1] = 1; order[2] = 2; }
    else     { order[0] = o0; order[1] = o1; order[2] = o2; }

    // index: 0 close, 1 max, 2 min
    int btn_x[3] = {0, 0, 0};

    int x = bx0;
    for (int slot = 0; slot < 3; slot++) {
        uint8_t which = order[slot];
        btn_x[which] = x;

        if (btn_right) x -= (btn + gap);
        else           x += (btn + gap);
    }

    // Hover detection
    bool hover_close = pt_in_rect(mx, my, btn_x[0], by, btn, btn);
    bool hover_max   = pt_in_rect(mx, my, btn_x[1], by, btn, btn);
    bool hover_min   = pt_in_rect(mx, my, btn_x[2], by, btn, btn);

    // Press şimdilik yok
    bool press_close = false, press_max = false, press_min = false;

    // --- Draw buttons
    if (th->window_btn_style == UI_BTN_STYLE_TRAFFIC) {
        uint32_t c_close = (uint32_t)th->window_btn_close;
        uint32_t c_max   = (uint32_t)th->window_btn_max;
        uint32_t c_min   = (uint32_t)th->window_btn_min;

        if (hover_close) c_close = darken_rgb(c_close, hover_dark);
        if (hover_max)   c_max   = darken_rgb(c_max,   hover_dark);
        if (hover_min)   c_min   = darken_rgb(c_min,   hover_dark);

        if (press_close) c_close = darken_rgb(c_close, press_dark);
        if (press_max)   c_max   = darken_rgb(c_max,   press_dark);
        if (press_min)   c_min   = darken_rgb(c_min,   press_dark);

        fb_draw_rect(btn_x[0], by, btn, btn, c_close);
        fb_draw_rect(btn_x[1], by, btn, btn, c_max);
        fb_draw_rect(btn_x[2], by, btn, btn, c_min);

        fb_draw_rect_outline(btn_x[0], by, btn, btn, col_border);
        fb_draw_rect_outline(btn_x[1], by, btn, btn, col_border);
        fb_draw_rect_outline(btn_x[2], by, btn, btn, col_border);
    } else {
        uint32_t base = darken_rgb(col_title_bg, 10);

        uint32_t b_close = base;
        uint32_t b_max   = base;
        uint32_t b_min   = base;

        if (hover_close) b_close = darken_rgb(b_close, hover_dark);
        if (hover_max)   b_max   = darken_rgb(b_max,   hover_dark);
        if (hover_min)   b_min   = darken_rgb(b_min,   hover_dark);

        if (press_close) b_close = darken_rgb(b_close, press_dark);
        if (press_max)   b_max   = darken_rgb(b_max,   press_dark);
        if (press_min)   b_min   = darken_rgb(b_min,   press_dark);

        fb_draw_rect(btn_x[0], by, btn, btn, b_close);
        fb_draw_rect(btn_x[1], by, btn, btn, b_max);
        fb_draw_rect(btn_x[2], by, btn, btn, b_min);

        fb_draw_rect_outline(btn_x[0], by, btn, btn, col_border);
        fb_draw_rect_outline(btn_x[1], by, btn, btn, col_border);
        fb_draw_rect_outline(btn_x[2], by, btn, btn, col_border);
    }

    // ------------------------------------------------------------
    // Title area safe rect (butonlarla çakışmasın)
    // ------------------------------------------------------------
    int safe_x = win->x + pad_l;
    int safe_w = win->w - (pad_l + pad_r);

    if (btn_right) {
        int right_block = mar + btn_total_w + btn_pad_r;
        safe_w = win->w - pad_l - right_block;
    } else {
        int left_block = mar + btn_total_w + btn_pad_l;
        safe_x = win->x + left_block;
        safe_w = win->w - left_block - pad_r;
    }

    if (safe_w < 0) safe_w = 0;

    // ------------------------------------------------------------
    // Tabs: title text yerine tab strip çiz
    // (WM: wm_add_window -> win->user_data = owner app)
    // ------------------------------------------------------------
    int drew_tabs = 0;

    app_t* app = (app_t*)win->user_data;
    if (app && app->v &&
        app->v->tabs_count && app->v->tabs_title &&
        app->v->tabs_active && app->v->tabs_set_active) {

        int n = app->v->tabs_count(app);
        if (n > 0) {
            ui_chrome_layout_t L = ui_chrome_layout(win);
            chrome_rect_t strip = chrome_tabstrip_rect(win, &L);

            // strip genişliği yoksa geç
            if (strip.w > 30) {
                int tab_h = L.btn_size;
                if (tab_h < 14) tab_h = 14;

                int tab_w = 90;
                int tab_gap = 6;

                int max_tabs = (n < 3) ? n : 3;

                int y = strip.y + (strip.h - tab_h) / 2;
                int x0 = strip.x;

                for (int i = 0; i < max_tabs; i++) {
                    // strip dışına taşmasın
                    if (x0 + tab_w > strip.x + strip.w) break;

                    const char* tt = app->v->tabs_title(app, i);
                    int active = (app->v->tabs_active(app) == i) ? 1 : 0;

                    draw_tab(x0, y, tab_w, tab_h, tt, active);
                    x0 += tab_w + tab_gap;
                }

                // [+]
                int add_w = 28;
                if (x0 + add_w <= strip.x + strip.w) {
                    draw_tab_add(x0, y, add_w, tab_h);
                }

                drew_tabs = 1;
            }
        }
    }

    // ------------------------------------------------------------
    // Title text (tabs yoksa)
    // ------------------------------------------------------------
    if (!drew_tabs) {
        if (win->title && win->title[0]) {
            int text_px_w = text_width8(win->title);
            int tx = calc_title_x(th, safe_x, safe_w, text_px_w);

            const int font_h = 16;
            int ty = win->y + (title_h - font_h) / 2;
            if (ty < win->y) ty = win->y;

            gfx_draw_text_utf8(tx, ty, col_title_text, win->title);
        }
    }
}