#include <ui/kbi_test.h>
#include <ui/session.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/fb_console.h>
#include <stdbool.h>
#include <stdint.h>

static bool g_inited = false;

static void draw_rect_fill(int x, int y, int w, int h, uint32_t color) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            fb_putpixel(x + xx, y + yy, color);
        }
    }
}

static void draw_rect_border(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < w; i++) {
        fb_putpixel(x + i, y, color);
        fb_putpixel(x + i, y + h - 1, color);
    }
    for (int i = 0; i < h; i++) {
        fb_putpixel(x, y + i, color);
        fb_putpixel(x + w - 1, y + i, color);
    }
}

static void kbi_test_redraw(void) {
    int sw = fb_get_width();
    int sh = fb_get_height();

    fb_clear(0x202020);

    int panel_w = 512;
    int panel_h = 512;
    int panel_x = (sw - panel_w) / 2;
    int panel_y = (sh - panel_h) / 2;

    draw_rect_fill(panel_x, panel_y, panel_w, panel_h, 0x000000);
    draw_rect_border(panel_x, panel_y, panel_w, panel_h, 0xFFFFFF);

    /* Şimdilik ortada test karesi */
    int box = 128;
    int bx = panel_x + (panel_w - box) / 2;
    int by = panel_y + (panel_h - box) / 2;

    draw_rect_fill(bx, by, box, box, 0xFF0000);
    draw_rect_border(bx, by, box, box, 0xFFFFFF);

    fb_present();
}

void kbi_test_init(void) {
    g_inited = true;
    kbi_test_redraw();
}

void kbi_test_tick(void) {
    if (!g_inited) return;
}

void kbi_test_handle_scancode(uint16_t sc) {
    uint8_t sc8 = (uint8_t)(sc & 0xFF);

    /* ESC örneği; sende break/make düzenine göre gerekirse düzelt */
    if (sc8 == 0x01) {
        ui_session_switch(UI_SESSION_DESKTOP);
    }
}