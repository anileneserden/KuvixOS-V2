#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <ui/font8x8_basic.h>
#include <lib/string.h>

static uint32_t g_fg = 0x00FFFFFF;
static uint32_t g_bg = 0x001A1A1A;

static int cur_x = 0;
static int cur_y = 0;

static const int CHAR_W = 8;
static const int CHAR_H = 8;

static int screen_w(void) { return (int)fb_get_width(); }
static int screen_h(void) { return (int)fb_get_height(); }

static void draw_char8(int x, int y, uint8_t ch, uint32_t fg, uint32_t bg) {
    const uint8_t* glyph = font8x8_basic[ch];

    for (int row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < 8; col++) {
            // gfx_draw_text ile aynı bit yönü:
            uint32_t color = (line & (1u << (7 - col))) ? fg : bg;
            gfx_putpixel(x + col, y + row, color);
        }
    }
}

static void newline(void) {
    cur_x = 0;
    cur_y += CHAR_H;

    if (cur_y + CHAR_H > screen_h()) {
        // Şimdilik basit: ekranı temizle ve başa sar
        gfx_clear(g_bg);
        cur_y = 0;
    }
}

void fb_console_init(uint32_t fg, uint32_t bg) {
    g_fg = fg;
    g_bg = bg;
    cur_x = 0;
    cur_y = 0;
    gfx_clear(g_bg);
    fb_present();
}

void fb_console_putc(char c) {
    if (c == '\r') return;

    if (c == '\n') { newline(); return; }

    if (c == '\b') {
        if (cur_x >= CHAR_W) cur_x -= CHAR_W;
        draw_char8(cur_x, cur_y, (uint8_t)' ', g_fg, g_bg);
        return;
    }

    if (cur_x + CHAR_W > screen_w()) newline();

    draw_char8(cur_x, cur_y, (uint8_t)c, g_fg, g_bg);
    cur_x += CHAR_W;
}

void fb_console_write(const char* s) {
    while (s && *s) fb_console_putc(*s++);
}

void fb_console_clear(void) {
    cur_x = 0;
    cur_y = 0;
    gfx_clear(g_bg);
    fb_present();
}

void fb_console_flush(void) {
    fb_present();
}