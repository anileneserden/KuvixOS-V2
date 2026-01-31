#include <ui/window.h>

#include <kernel/drivers/video/fb.h>
#include <ui/theme.h>
#include <ui/font8x8_basic.h>

#define TITLE_H   24

#define BTN_SIZE  10
#define BTN_PAD_X 6
#define BTN_GAP   6
#define BTN_Y     7   // titlebar içinde y

/* ---------------- text ---------------- */
static void draw_text8(int x, int y, uint32_t color, const char* s)
{
    while (*s) {
        unsigned char c = (unsigned char)*s;
        const uint8_t* glyph = font8x8_basic[c];

        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) {
                    fb_putpixel(x + col, y + row, color);
                }
            }
        }

        x += 8;
        s++;
    }
}

static int kstrlen(const char* s)
{
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

/* ---------------- util ---------------- */
// argb = 0xAARRGGBB
static uint32_t darken(uint32_t argb, int amount)
{
    uint8_t a = (argb >> 24) & 0xFF;
    int r = (argb >> 16) & 0xFF;
    int g = (argb >> 8)  & 0xFF;
    int b = (argb >> 0)  & 0xFF;

    r -= amount; if (r < 0) r = 0;
    g -= amount; if (g < 0) g = 0;
    b -= amount; if (b < 0) b = 0;

    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fb_fill_circle(int cx, int cy, int r, uint32_t col)
{
    int rr = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= rr) {
                fb_putpixel(cx + dx, cy + dy, col);
            }
        }
    }
}

static void fb_circle_outline(int cx, int cy, int r, uint32_t col)
{
    int rr = r * r;
    int r2 = (r - 1);
    r2 = r2 * r2;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d = dx*dx + dy*dy;
            if (d <= rr && d >= r2) {
                fb_putpixel(cx + dx, cy + dy, col);
            }
        }
    }
}

/* ---------------- grip ---------------- */
static void draw_resize_grip(const ui_window_t* win, uint32_t col)
{
    // sağ-alt köşeye 3 adet diyagonal çizgi (10x10 içinde)
    int gx = win->x + win->w - 1;
    int gy = win->y + win->h - 1;

    for (int i = 0; i < 3; i++) {
        int off = 2 + i * 3;

        fb_putpixel(gx - off,         gy,             col);
        fb_putpixel(gx - off + 1,     gy - 1,         col);

        fb_putpixel(gx,               gy - off,       col);
        fb_putpixel(gx - 1,           gy - off + 1,   col);

        fb_putpixel(gx - off + 2,     gy - 2,         col);
    }
}

/* ---------------- drawing ---------------- */
static void draw_frame(const ui_window_t* win, const ui_theme_t* th, int is_active)
{
    uint32_t border_col = th->window_border;
    if (is_active) border_col = fb_rgb(80,120,220);

    fb_draw_rect(win->x, win->y, win->w, win->h, th->window_bg);
    fb_draw_rect_outline(win->x, win->y, win->w, win->h, border_col);
}

static void draw_titlebar_centered(const ui_window_t* win, const ui_theme_t* th)
{
    fb_draw_rect(win->x, win->y, win->w, TITLE_H, th->window_title_bg);

    if (!win->title) return;

    int len = kstrlen(win->title);
    int text_w = len * 8;

    int cx = win->x + (win->w / 2);
    int x  = cx - (text_w / 2);

    // dikey ortalama (8px font)
    int y  = win->y + (TITLE_H - 8) / 2;

    // Butonlar solda yer kaplıyor: close+min+max alanının sağına clamp
    int left_limit = win->x + BTN_PAD_X + (BTN_SIZE * 3) + (BTN_GAP * 2) + 8;
    if (x < left_limit) x = left_limit;

    draw_text8(x, y, th->window_title_text, win->title);
}

static void draw_title_buttons_round(const ui_window_t* win, const ui_theme_t* th)
{
    (void)th;

    int bx = win->x + BTN_PAD_X;
    int by = win->y + BTN_Y;

    int step = BTN_SIZE + BTN_GAP;
    int cy   = by + (BTN_SIZE / 2);

    // mac: close, min, max (soldan sağa)
    int cx_close = bx + (BTN_SIZE / 2) + step * 0;
    int cx_min   = bx + (BTN_SIZE / 2) + step * 1;
    int cx_max   = bx + (BTN_SIZE / 2) + step * 2;

    int r = (BTN_SIZE / 2) - 1;

    // mac-ish renkler
    uint32_t col_close = fb_rgb(255,  95,  86);
    uint32_t col_min   = fb_rgb(255, 189,  46);
    uint32_t col_max   = fb_rgb( 39, 201,  63);

    // hafif border (daha buton gibi)
    uint32_t bd_close = darken(col_close, 40);
    uint32_t bd_min   = darken(col_min,   40);
    uint32_t bd_max   = darken(col_max,   40);

    fb_fill_circle(cx_close, cy, r, col_close);
    fb_circle_outline(cx_close, cy, r, bd_close);

    fb_fill_circle(cx_min, cy, r, col_min);
    fb_circle_outline(cx_min, cy, r, bd_min);

    fb_fill_circle(cx_max, cy, r, col_max);
    fb_circle_outline(cx_max, cy, r, bd_max);
}

void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my)
{
    (void)mx;
    (void)my;

    if (!win) return;

    const ui_theme_t* th = ui_get_theme();

    // frame + titlebar
    draw_frame(win, th, is_active);
    draw_titlebar_centered(win, th);

    // grip
    uint32_t border_col = th->window_border;
    if (is_active) border_col = fb_rgb(80,120,220);

    uint32_t grip_col = darken(border_col, 10);
    draw_resize_grip(win, grip_col);

    // title buttons (round, no hover/hittest)
    draw_title_buttons_round(win, th);
}