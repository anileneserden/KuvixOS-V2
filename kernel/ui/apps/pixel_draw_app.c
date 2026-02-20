// kernel/ui/apps/pixel_draw_app.c
//
// Pixel Draw (mini pixel editor)
// - Üst toolbar (panel) + "Clear" butonu
// - Toolbar içinde palette kutuları (tıklayınca renk seçer)
// - Canvas: checkerboard (gri/beyaz) + boyanan hücreler
// - Grid çizgisi yok, sadece hover hücresi siyah border
// - Sol tık: boya, Sağ tık: sil (alpha=0)
// - CTRL + Wheel: zoom in/out (cell_px değişir)
//
// ÖNEMLİ:
// - pixel_draw_app_t ve palette_color_t header'da TANIMLI olmalı.
// - app_manager.c tarafında data_size = sizeof(pixel_draw_app_t) olmalı.
// - .c içinde typedef struct tekrar ETMEYİN.

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/messagebox.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

#include <ui/apps/pixel_draw_app.h>   // ✅ struct burada

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static void draw_rect_1px(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

static void draw_checker_bg(int x, int y, int w, int h, int checker_px) {
    uint32_t a = 0xFFE8E8E8; // açık gri
    uint32_t b = 0xFFFFFFFF; // beyaz

    for (int yy = 0; yy < h; yy += checker_px) {
        for (int xx = 0; xx < w; xx += checker_px) {
            int gx = xx / checker_px;
            int gy = yy / checker_px;
            uint32_t col = (((gx + gy) & 1) == 0) ? a : b;

            int cw = (xx + checker_px <= w) ? checker_px : (w - xx);
            int ch = (yy + checker_px <= h) ? checker_px : (h - yy);
            if (cw > 0 && ch > 0)
                gfx_fill_rect(x + xx, y + yy, cw, ch, col);
        }
    }
}

// WM: çizimde origin set -> (0,0) client.
// Input: ekran koordinatı -> client'e çevriliyor.
static void compute_layout(pixel_draw_app_t* p) {
    ui_rect_t c = wm_get_client_rect(p->window_id);

    // toolbar yüksekliği
    p->toolbar_h = 42;

    // canvas boyutu
    p->canvas_w_px = p->cells_w * p->cell_px;
    p->canvas_h_px = p->cells_h * p->cell_px;

    // toolbar altında ortala
    int usable_h = c.h - p->toolbar_h;
    if (usable_h < 0) usable_h = 0;

    p->canvas_x = (c.w - p->canvas_w_px) / 2;
    p->canvas_y = p->toolbar_h + (usable_h - p->canvas_h_px) / 2;

    if (p->canvas_x < 8) p->canvas_x = 8;
    if (p->canvas_y < p->toolbar_h + 8) p->canvas_y = p->toolbar_h + 8;

    // UI layout
    p->root.base.location    = (ui_point_t){ 0, 0 };
    p->root.base.size        = (ui_size_t){ c.w, c.h };

    p->toolbar.base.location = (ui_point_t){ 0, 0 };
    p->toolbar.base.size     = (ui_size_t){ c.w, p->toolbar_h };

    // Clear butonu sağ üst
    p->btn_clear.base.size     = (ui_size_t){ 80, 26 };
    p->btn_clear.base.location = (ui_point_t){ c.w - 80 - 10, 8 };
}

static bool screen_to_cell(pixel_draw_app_t* p, int lx, int ly, int* out_cx, int* out_cy) {
    int rx = lx - p->canvas_x;
    int ry = ly - p->canvas_y;
    if (rx < 0 || ry < 0) return false;
    if (rx >= p->canvas_w_px || ry >= p->canvas_h_px) return false;

    int cx = rx / p->cell_px;
    int cy = ry / p->cell_px;
    if (cx < 0 || cy < 0 || cx >= p->cells_w || cy >= p->cells_h) return false;

    *out_cx = cx;
    *out_cy = cy;
    return true;
}

static void clear_canvas(pixel_draw_app_t* p) {
    if (!p->pixels) return;
    memset(p->pixels, 0, sizeof(uint32_t) * (uint32_t)(p->cells_w * p->cells_h));
}

// Palette rectangles (toolbar sol)
static void palette_get_rect(pixel_draw_app_t* p, int index, int* x, int* y, int* w, int* h) {
    (void)p;
    int start_x = 12;
    int start_y = 8;
    int box = 24;
    int gap = 8;

    *x = start_x + index * (box + gap);
    *y = start_y;
    *w = box;
    *h = box;
}

static int palette_hit_test(pixel_draw_app_t* p, int lx, int ly) {
    for (int i = 0; i < p->palette_count; i++) {
        int x, y, w, h;
        palette_get_rect(p, i, &x, &y, &w, &h);
        if (lx >= x && ly >= y && lx < x + w && ly < y + h) return i;
    }
    return -1;
}

static void draw_palette(pixel_draw_app_t* p) {
    for (int i = 0; i < p->palette_count; i++) {
        int x, y, w, h;
        palette_get_rect(p, i, &x, &y, &w, &h);

        gfx_fill_rect(x, y, w, h, p->palette[i].rgba);
        draw_rect_1px(x, y, w, h, 0xFF404040);

        if (i == p->selected_color) {
            draw_rect_1px(x - 2, y - 2, w + 4, h + 4, 0xFF000000);
        }
    }
}

static void draw_canvas(pixel_draw_app_t* p) {
    // checker: grid ile aynı boyutta olsun => checker_px = cell_px
    draw_checker_bg(p->canvas_x, p->canvas_y,
                    p->canvas_w_px, p->canvas_h_px,
                    p->cell_px);

    // pixels
    int pad = p->pixel_pad;
    int pix = p->pixel_px;

    for (int cy = 0; cy < p->cells_h; cy++) {
        for (int cx = 0; cx < p->cells_w; cx++) {
            uint32_t col = p->pixels[cy * p->cells_w + cx];
            if ((col >> 24) == 0) continue;

            int px = p->canvas_x + cx * p->cell_px + pad;
            int py = p->canvas_y + cy * p->cell_px + pad;
            gfx_fill_rect(px, py, pix, pix, col);
        }
    }

    // hover border
    if (p->hover_valid) {
        int hx = p->canvas_x + p->hover_x * p->cell_px;
        int hy = p->canvas_y + p->hover_y * p->cell_px;
        draw_rect_1px(hx, hy, p->cell_px, p->cell_px, 0xFF000000);
    }

    // canvas border
    draw_rect_1px(p->canvas_x - 2, p->canvas_y - 2, p->canvas_w_px + 4, p->canvas_h_px + 4, 0xFF2A2A2A);
    draw_rect_1px(p->canvas_x - 1, p->canvas_y - 1, p->canvas_w_px + 2, p->canvas_h_px + 2, 0xFF6A6A6A);
}

// ------------------------------------------------------------
// UI callbacks
// ------------------------------------------------------------
static void on_click_clear(void* user) {
    pixel_draw_app_t* p = (pixel_draw_app_t*)user;
    clear_canvas(p);
}

// ------------------------------------------------------------
// App callbacks
// ------------------------------------------------------------
static void pixel_draw_on_create(app_t* self) {
    pixel_draw_app_t* p = (pixel_draw_app_t*)self->user;
    p->window_id = self->win_id;

    // ctrl state
    p->ctrl_down = false;

    // canvas settings
    p->cell_px   = 24;
    p->pixel_pad = 0;
    p->pixel_px  = p->cell_px; // pad=0 => aynı

    p->cells_w = 32;
    p->cells_h = 24;

    // palette
    p->palette_count = 5;
    p->palette[0] = (palette_color_t){ .rgba = 0xFF00FF00, .name = "Green" }; // 0,255,0
    p->palette[1] = (palette_color_t){ .rgba = 0xFFFF0000, .name = "Red"   }; // 255,0,0
    p->palette[2] = (palette_color_t){ .rgba = 0xFF0000FF, .name = "Blue"  }; // 0,0,255
    p->palette[3] = (palette_color_t){ .rgba = 0xFF000000, .name = "Black" }; // 0,0,0
    p->palette[4] = (palette_color_t){ .rgba = 0xFFFFFFFF, .name = "White" }; // 255,255,255

    p->selected_color = 3;
    p->draw_color = p->palette[p->selected_color].rgba;

    // pixels buffer
    uint32_t count = (uint32_t)(p->cells_w * p->cells_h);
    p->pixels = (uint32_t*)kmalloc(sizeof(uint32_t) * count);
    if (p->pixels) memset(p->pixels, 0, sizeof(uint32_t) * count);

    p->hover_valid = false;

    // init UI
    ui_ctx_init(&p->ui);

    ui_panel2_init(&p->root,    1, (ui_point_t){0,0}, (ui_size_t){0,0}, 0x1E1E1E);
    ui_panel2_init(&p->toolbar, 2, (ui_point_t){0,0}, (ui_size_t){0,0}, 0x2A2A2A);
    ui_panel2_set_border(&p->toolbar, 1, 0x3A3A3A);

    ui_button2_init(&p->btn_clear, 3, (ui_point_t){0,0}, (ui_size_t){80,26}, "Clear");
    ui_button2_onclick(&p->btn_clear, on_click_clear, p);

    ui_control_add_child(&p->root.base, &p->toolbar.base);
    ui_control_add_child(&p->root.base, &p->btn_clear.base);

    ui_ctx_add_root(&p->ui, &p->root.base);

    compute_layout(p);
}

static void pixel_draw_on_draw(app_t* self) {
    pixel_draw_app_t* p = (pixel_draw_app_t*)self->user;

    compute_layout(p);

    // UI (root + toolbar + clear)
    ui_ctx_draw(&p->ui);

    // palette (toolbar üstüne)
    draw_palette(p);

    // canvas
    draw_canvas(p);
}

static void pixel_draw_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t e1, uint8_t e2) {
    pixel_draw_app_t* p = (pixel_draw_app_t*)self->user;

    if (messagebox_is_visible()) return;
    if (wm_is_any_window_captured()) return;

    compute_layout(p);

    // ekran -> client-relative
    ui_rect_t c = wm_get_client_rect(p->window_id);
    int lx = mx - c.x;
    int ly = my - c.y;

    bool ldown = (buttons & 1) != 0;
    bool rdown = (buttons & 2) != 0; // sağ tuş maskesi sende farklıysa değiştir

    // UI'ye ver (Clear click)
    ui_ctx_mouse(&p->ui, lx, ly, ldown);

    // Palette click (sol)
    if (ldown) {
        int hit = palette_hit_test(p, lx, ly);
        if (hit >= 0) {
            p->selected_color = hit;
            p->draw_color = p->palette[p->selected_color].rgba;
            return; // palette tıkladıysa canvas çizme
        }
    }

    // ---------------------------
    // CTRL + Wheel Zoom
    // ---------------------------
    // e1/e2 işaretli delta olabilir; int8 cast ile yönü alıyoruz.
    int wheel = 0;
    if (e2 != 0) wheel = (int)(int8_t)e2;
    else if (e1 != 0) wheel = (int)(int8_t)e1;

    if (p->ctrl_down && wheel != 0) {
        int step = 2;
        int min_cell = 8;
        int max_cell = 64;

        if (wheel > 0) p->cell_px += step;
        else           p->cell_px -= step;

        if (p->cell_px < min_cell) p->cell_px = min_cell;
        if (p->cell_px > max_cell) p->cell_px = max_cell;

        // pixel = cell (pad=0)
        p->pixel_pad = 0;
        p->pixel_px  = p->cell_px;

        compute_layout(p);
        return;
    }

    printk("wheel=%d e1=%d e2=%d\n", wheel, e1, e2);

    // Canvas hover + paint
    int cx, cy;
    bool inside = screen_to_cell(p, lx, ly, &cx, &cy);

    if (inside) {
        p->hover_valid = true;
        p->hover_x = cx;
        p->hover_y = cy;
    } else {
        p->hover_valid = false;
    }

    if (inside && p->pixels) {
        int idx = cy * p->cells_w + cx;
        if (ldown) {
            p->pixels[idx] = p->draw_color;
        } else if (rdown) {
            p->pixels[idx] = 0x00000000; // şeffaf
        }
    }
}

static void pixel_draw_on_key(app_t* self, uint16_t sc) {
    pixel_draw_app_t* p = (pixel_draw_app_t*)self->user;

    // Sol Ctrl
    if (sc == 0x001D) { p->ctrl_down = true;  return; }
    if (sc == 0x009D) { p->ctrl_down = false; return; }

    // Sağ Ctrl (extended)
    if (sc == 0xE01D) { p->ctrl_down = true;  return; }
    if (sc == 0xE09D) { p->ctrl_down = false; return; }
}

static void pixel_draw_on_destroy(app_t* self) {
    (void)self;
    // kfree yoksa boş bırak.
    // Eğer ileride kfree gelirse: kfree(p->pixels);
}

const app_vtbl_t pixel_draw_app_vtbl = {
    .on_create = pixel_draw_on_create,
    .on_draw   = pixel_draw_on_draw,
    .on_key    = pixel_draw_on_key,
    .on_mouse  = pixel_draw_on_mouse,
    .on_destroy= pixel_draw_on_destroy,
    .on_close_request = 0
};