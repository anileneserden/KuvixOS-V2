// kernel/drivers/video/gfx.c
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <ui/font/font8x16_basic.h>
#include <stdint.h>

static int g_origin_x = 0;
static int g_origin_y = 0;

static inline int OX(int x) { return x + g_origin_x; }
static inline int OY(int y) { return y + g_origin_y; }

void gfx_init(void) {
    fb_clear(0x000000);
    fb_present();
}

void gfx_clear(uint32_t color) {
    fb_clear(color);
}

void gfx_set_origin(int x, int y) {
    g_origin_x = x;
    g_origin_y = y;
}

void gfx_reset_origin(void) {
    g_origin_x = 0;
    g_origin_y = 0;
}

void gfx_putpixel(int x, int y, uint32_t color) {
    fb_putpixel(OX(x), OY(y), color);
}

/* alpha blend pixel: ORIGIN + clip */
void gfx_putpixel_alpha(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int ax = OX(x);
    int ay = OY(y);

    if (ax < 0 || ay < 0 || (uint32_t)ax >= fb_get_width() || (uint32_t)ay >= fb_get_height())
        return;

    fb_color_t bg = fb_getpixel(ax, ay);

    uint8_t bg_r = (bg >> 16) & 0xFF;
    uint8_t bg_g = (bg >> 8)  & 0xFF;
    uint8_t bg_b = (bg >> 0)  & 0xFF;

    uint8_t out_r = (uint8_t)(((uint32_t)r * a + (uint32_t)bg_r * (255 - a)) / 255);
    uint8_t out_g = (uint8_t)(((uint32_t)g * a + (uint32_t)bg_g * (255 - a)) / 255);
    uint8_t out_b = (uint8_t)(((uint32_t)b * a + (uint32_t)bg_b * (255 - a)) / 255);

    uint32_t final = ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | (uint32_t)out_b;
    fb_putpixel(ax, ay, final);
}

void gfx_draw_alpha_rect(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int x, int y) {
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            gfx_putpixel_alpha(x + xx, y + yy, r, g, b, a);
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
            gfx_putpixel(x + xx, y + yy, color);
}

#define ABS(x) ((x) < 0 ? -(x) : (x))

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    // Bresenham (client coords), gfx_putpixel handles origin
    int dx = ABS(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -ABS(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        gfx_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    for (int xx = 0; xx < w; xx++) {
        gfx_putpixel(x + xx, y,         color);
        gfx_putpixel(x + xx, y + h - 1, color);
    }
    for (int yy = 0; yy < h; yy++) {
        gfx_putpixel(x,         y + yy, color);
        gfx_putpixel(x + w - 1, y + yy, color);
    }
}

/* sqrt helper */
static uint32_t isqrt_u32(uint32_t n) {
    uint32_t x = n;
    uint32_t y = (x + 1) >> 1;
    while (y < x) { x = y; y = (x + n / x) >> 1; }
    return x;
}

void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    int maxr = (w < h ? w : h) / 2;
    if (r > maxr) r = maxr;

    const int rr = r * r;

    for (int yy = 0; yy < h; yy++) {
        int inset = 0;
        if (r > 0) {
            if (yy < r) {
                int dy = (r - 1) - yy;
                inset = r - (int)isqrt_u32((uint32_t)(rr - dy * dy));
            } else if (yy >= h - r) {
                int dy = yy - (h - r);
                inset = r - (int)isqrt_u32((uint32_t)(rr - dy * dy));
            }
        }

        int x0 = x + inset;
        int x1 = x + w - inset - 1;
        for (int xx = x0; xx <= x1; xx++) {
            gfx_putpixel(xx, y + yy, color);
        }
    }
}

void gfx_draw_text(int x, int y, uint32_t color, const char* s) {
    if (!s) return;

    while (*s) {
        uint8_t c = (uint8_t)*s++;

        for (int row = 0; row < 16; row++) {
            uint8_t line = font8x16_basic_row(c, row);
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) {
                    gfx_putpixel(x + col, y + row, color);
                }
            }
        }
        x += 8;
    }
}

/* --- UTF-8 decode --- */
static uint32_t utf8_next(const char** ps) {
    const unsigned char* s = (const unsigned char*)(*ps);
    if (!*s) return 0;

    uint32_t cp = 0;
    if (s[0] < 0x80) { cp = s[0]; *ps += 1; return cp; }

    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        *ps += 2; return cp;
    }

    if ((s[0] & 0xF0) == 0xE0 &&
        (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        *ps += 3; return cp;
    }

    *ps += 1;
    return '?';
}

static uint8_t unicode_to_kvx_byte(uint32_t cp) {
    if (cp < 0x80) return (uint8_t)cp;

    switch (cp) {
        case 0x00FC: return 0xFC; // ü
        case 0x00DC: return 0xDC; // Ü
        case 0x00F6: return 0xF6; // ö
        case 0x00D6: return 0xD6; // Ö
        case 0x00E7: return 0xE7; // ç
        case 0x00C7: return 0xC7; // Ç
        case 0x011F: return 0xF0; // ğ
        case 0x011E: return 0xD0; // Ğ
        case 0x015F: return 0xFE; // ş
        case 0x015E: return 0xDE; // Ş
        case 0x0131: return 0xFD; // ı
        case 0x0130: return 0xDD; // İ
        case 0x00D7: return 0xF7; // ×
        case 0x00F7: return 0xF8; // ÷
        default:     return '?';
    }
}

void gfx_draw_text_utf8(int x, int y, uint32_t color, const char* s) {
    if (!s) return;

    char out[256];
    int oi = 0;

    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (!cp) break;

        uint8_t ch = unicode_to_kvx_byte(cp);

        if (ch == '\n' || ch == '\r') {
            out[oi] = '\0';
            if (oi) gfx_draw_text(x, y, color, out);
            oi = 0;
            y += 16;
            continue;
        }

        out[oi++] = (char)ch;

        if (oi >= (int)sizeof(out) - 1) {
            out[oi] = '\0';
            gfx_draw_text(x, y, color, out);
            oi = 0;
        }
    }

    out[oi] = '\0';
    if (oi) gfx_draw_text(x, y, color, out);
}