// kernel/drivers/video/fb_console.c
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>

#include <ui/font8x16_basic.h>
#include <ui/font8x8_basic.h>   // sadece "glyph boş mu?" kontrolü için

#include <stdint.h>

static uint32_t g_fg = 0x00FFFFFF;
static uint32_t g_bg = 0x001A1A1A;

static int cur_x = 0;
static int cur_y = 0;

static const int CHAR_W = 8;
static const int CHAR_H = 16;

static int screen_w(void) { return (int)fb_get_width(); }
static int screen_h(void) { return (int)fb_get_height(); }

int fb_console_cols(void) {
    int w = screen_w();
    return (w <= 0) ? 0 : (w / CHAR_W);
}

int fb_console_rows(void) {
    int h = screen_h();
    return (h <= 0) ? 0 : (h / CHAR_H);
}

void fb_console_set_cursor(int col, int row) {
    if (col < 0) col = 0;
    if (row < 0) row = 0;

    int maxc = fb_console_cols();
    int maxr = fb_console_rows();

    if (maxc > 0 && col >= maxc) col = maxc - 1;
    if (maxr > 0 && row >= maxr) row = maxr - 1;

    cur_x = col * CHAR_W;
    cur_y = row * CHAR_H;
}

void fb_console_get_cursor(int* out_col, int* out_row) {
    if (out_col) *out_col = (cur_x / CHAR_W);
    if (out_row) *out_row = (cur_y / CHAR_H);
}

static int glyph8_is_empty(uint8_t ch) {
    const uint8_t* g = font8x8_basic[ch];
    // 8 satırın hepsi 0 mı?
    return (g[0] | g[1] | g[2] | g[3] | g[4] | g[5] | g[6] | g[7]) == 0;
}

static void draw_char16(int x, int y, uint8_t ch, uint32_t fg, uint32_t bg) {
    for (int row = 0; row < 16; row++) {
        uint8_t line = font8x16_basic_row(ch, row);
        for (int col = 0; col < 8; col++) {
            uint32_t color = (line & (1u << (7 - col))) ? fg : bg;
            gfx_putpixel(x + col, y + row, color);
        }
    }
}

static void newline(void) {
    cur_x = 0;
    cur_y += CHAR_H;

    if (cur_y + CHAR_H > screen_h()) {
        // Basit davranış: ekranı temizle ve başa sar
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
    // CR yok say
    if (c == '\r') return;

    // Newline
    if (c == '\n') {
        newline();
        return;
    }

    // Tab: 4 boşluk
    if (c == '\t') {
        for (int i = 0; i < 4; i++) fb_console_putc(' ');
        return;
    }

    // Backspace / DEL
    if (c == '\b' || (uint8_t)c == 127) {
        if (cur_x >= CHAR_W)
            cur_x -= CHAR_W;
        else
            cur_x = 0;

        // ✅ 16px yüksekliğe göre boşluk çiz
        draw_char16(cur_x, cur_y, (uint8_t)' ', g_fg, g_bg);
        return;
    }

    uint8_t uc = (uint8_t)c;

    // Kontrol karakterlerini '?' yap (ASCII dışını değil!)
    if (uc < 32) uc = (uint8_t)'?';

    // ✅ glyph tamamen boşsa '?' fallback (space hariç)
    if (uc != (uint8_t)' ' && glyph8_is_empty(uc)) {
        uc = (uint8_t)'?';
    }

    // Satır sonu taşarsa newline
    if (cur_x + CHAR_W > screen_w()) {
        newline();
    }

    // ✅ 8x16 çiz
    draw_char16(cur_x, cur_y, uc, g_fg, g_bg);
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