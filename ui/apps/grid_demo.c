// ui/apps/grid_demo_app.c

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/dialogs/messagebox.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int window_id;
} grid_demo_app_t;

// --- küçük yardımcılar ---
// Sende gfx_draw_rect yoksa 1px border'ı fill_rect ile çiziyoruz.
static void draw_rect_1px(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

// 50x50 karelerden oluşan checkerboard
static void draw_checkerboard(int x, int y, int w, int h, int cell, uint32_t white, uint32_t black) {
    // beyaz zemin
    gfx_fill_rect(x, y, w, h, white);

    // siyah kareler
    for (int yy = 0; yy < h; yy += cell) {
        for (int xx = 0; xx < w; xx += cell) {
            int gx = xx / cell;
            int gy = yy / cell;
            if (((gx + gy) & 1) == 1) {
                // son hücrelerde taşmayı önlemek için clamp
                int cw = (xx + cell <= w) ? cell : (w - xx);
                int ch = (yy + cell <= h) ? cell : (h - yy);
                if (cw > 0 && ch > 0) {
                    gfx_fill_rect(x + xx, y + yy, cw, ch, black);
                }
            }
        }
    }
}

static void draw_grid_lines(int x, int y, int w, int h, int cell, uint32_t bg, uint32_t line)
{
    // Beyaz zemin
    gfx_fill_rect(x, y, w, h, bg);

    // Dikey çizgiler
    for (int xx = 0; xx <= w; xx += cell) {
        gfx_fill_rect(x + xx, y, 1, h, line);
    }

    // Yatay çizgiler
    for (int yy = 0; yy <= h; yy += cell) {
        gfx_fill_rect(x, y + yy, w, 1, line);
    }
}

static void grid_demo_layout(int* out_canvas_x, int* out_canvas_y, int* out_canvas_w, int* out_canvas_h, int win_id) {
    ui_rect_t c = wm_get_client_rect(win_id);

    // Canvas boyutu: sabit veya client'a göre ölçeklenebilir.
    // Şimdilik sabit, client küçükse kırp.
    int canvas_w = 600;
    int canvas_h = 400;

    if (canvas_w > c.w - 40) canvas_w = c.w - 40;
    if (canvas_h > c.h - 40) canvas_h = c.h - 40;
    if (canvas_w < 50) canvas_w = (c.w > 0) ? c.w : 50;
    if (canvas_h < 50) canvas_h = (c.h > 0) ? c.h : 50;

    int canvas_x = (c.w - canvas_w) / 2;
    int canvas_y = (c.h - canvas_h) / 2;

    *out_canvas_x = canvas_x;
    *out_canvas_y = canvas_y;
    *out_canvas_w = canvas_w;
    *out_canvas_h = canvas_h;
}

static void grid_demo_on_create(app_t* self) {
    grid_demo_app_t* g = (grid_demo_app_t*)self->user;
    g->window_id = self->win_id;
}

static void grid_demo_on_draw(app_t* self) {
    grid_demo_app_t* g = (grid_demo_app_t*)self->user;

    // WM origin set ediyor: burada (0,0) client alanının sol üstü gibi davranabiliriz.
    ui_rect_t c = wm_get_client_rect(g->window_id);

    // 1) Koyu arka plan (client alanı)
    uint32_t bg = 0xFF1E1E1E;
    gfx_fill_rect(0, 0, c.w, c.h, bg);

    // 2) Ortada beyaz canvas + 50x50 siyah kareler
    int cx, cy, cw, ch;
    grid_demo_layout(&cx, &cy, &cw, &ch, g->window_id);

    uint32_t white = 0xFFFFFFFF;
    uint32_t black = 0xFF000000;

    draw_grid_lines(cx, cy, cw, ch, 10, 0xFFFFFFFF, 0xFF000000);
    // draw_checkerboard(cx, cy, cw, ch, 50, white, black);

    // 3) Canvas border (görünsün diye)
    draw_rect_1px(cx - 2, cy - 2, cw + 4, ch + 4, 0xFF101010);
    draw_rect_1px(cx - 1, cy - 1, cw + 2, ch + 2, 0xFF303030);

    // İstersen burada ayrıca “grid çizgisi” overlay de çizeriz.
    // Şimdilik kare desen zaten grid hissi veriyor.
}

static void grid_demo_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t e1, uint8_t e2) {
    (void)self; (void)e1; (void)e2; (void)mx; (void)my; (void)buttons;

    // Şimdilik sadece görüntü. İleride:
    // - client-relative dönüşüm (demo_app gibi)
    // - canvas'a çizim
    // - zoom/pan
    // ekleyeceğiz.

    if (messagebox_is_visible()) return;
    if (wm_is_any_window_captured()) return;
}

static void grid_demo_on_key(app_t* self, uint16_t sc) { (void)self; (void)sc; }
static void grid_demo_on_destroy(app_t* self) { (void)self; }

const app_vtbl_t grid_demo_app_vtbl = {
    .on_create = grid_demo_on_create,
    .on_draw   = grid_demo_on_draw,
    .on_key    = grid_demo_on_key,
    .on_mouse  = grid_demo_on_mouse,
    .on_destroy= grid_demo_on_destroy,
    .on_close_request = 0
};