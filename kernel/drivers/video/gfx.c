#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <ui/font/font8x8_basic.h>
#include <ui/font/font8x16_basic.h>

static int g_origin_x = 0;
static int g_origin_y = 0;

static uint32_t isqrt_u32(uint32_t n);
static inline int in_clip(int sx, int sy);

void gfx_init(void) {
    // Ekranı başlangıç için siyahla temizle
    fb_clear(0x000000); 
    // Temizlenen siyah ekranı donanıma gönder
    fb_present();       
}

// Tüm ekranı bir ARGB rengiyle temizler
void gfx_clear(uint32_t color) {
    fb_clear(color);
}

// Temel piksel çizimi
void gfx_putpixel(int x, int y, uint32_t color) {
    int sx = x + g_origin_x;
    int sy = y + g_origin_y;

    if (!in_clip(sx, sy)) return;

    fb_putpixel(sx, sy, color);
}

// r, g, b: Yeni rengin bileşenleri
// a: Saydamlık (0-255 arası, 0 tam saydam, 255 tam mat)
void gfx_putpixel_alpha(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // ✅ origin uygula
    int sx = x + g_origin_x;
    int sy = y + g_origin_y;

    if (sx < 0 || sy < 0 || (uint32_t)sx >= fb_get_width() || (uint32_t)sy >= fb_get_height()) return;

    // ✅ arka planı origin'li koordinattan oku
    fb_color_t bg_color = fb_getpixel(sx, sy);

    uint8_t bg_r = (bg_color >> 16) & 0xFF;
    uint8_t bg_g = (bg_color >> 8) & 0xFF;
    uint8_t bg_b = bg_color & 0xFF;

    uint8_t out_r = ((r * a) + (bg_r * (255 - a))) / 255;
    uint8_t out_g = ((g * a) + (bg_g * (255 - a))) / 255;
    uint8_t out_b = ((b * a) + (bg_b * (255 - a))) / 255;

    uint32_t final_color = (out_r << 16) | (out_g << 8) | out_b;

    // ✅ origin'li koordinata yaz
    fb_putpixel(sx, sy, final_color);
}

// İstediğin özel parametre sıralamasıyla Alpha Rect
void gfx_draw_alpha_rect(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int x, int y) {
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            // Mevcut pikselleri alpha ile harmanlayarak boyar
            gfx_putpixel_alpha(x + xx, y + yy, r, g, b, a);
        }
    }
}

// Kare/Dikdörtgen çizimi
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int yy=0; yy<h; yy++)
        for (int xx=0; xx<w; xx++)
            gfx_putpixel(x+xx, y+yy, color);
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void gfx_fill_round_rect4(int x, int y, int w, int h,
                          int rtl, int rtr, int rbl, int rbr,
                          uint32_t color) {
    if (w <= 0 || h <= 0) return;

    int maxr = (w < h ? w : h) / 2;
    rtl = clampi(rtl, 0, maxr);
    rtr = clampi(rtr, 0, maxr);
    rbl = clampi(rbl, 0, maxr);
    rbr = clampi(rbr, 0, maxr);

    for (int yy = 0; yy < h; yy++) {

        int inset_l = 0;
        int inset_r = 0;

        // ---- LEFT side inset (TL or BL) ----
        if (yy < rtl && rtl > 0) {
            int r = rtl;
            int dy = (r - 1) - yy;
            int rr = r * r;
            inset_l = r - (int)isqrt_u32((uint32_t)(rr - dy * dy));
        } else if (yy >= h - rbl && rbl > 0) {
            int r = rbl;
            int dy = yy - (h - r);
            int rr = r * r;
            inset_l = r - (int)isqrt_u32((uint32_t)(rr - dy * dy));
        }

        // ---- RIGHT side inset (TR or BR) ----
        if (yy < rtr && rtr > 0) {
            int r = rtr;
            int dy = (r - 1) - yy;
            int rr = r * r;
            inset_r = r - (int)isqrt_u32((uint32_t)(rr - dy * dy));
        } else if (yy >= h - rbr && rbr > 0) {
            int r = rbr;
            int dy = yy - (h - r);
            int rr = r * r;
            inset_r = r - (int)isqrt_u32((uint32_t)(rr - dy * dy));
        }

        int x0 = x + inset_l;
        int x1 = x + w - 1 - inset_r;

        if (x1 < x0) continue;

        // satırı doldur
        for (int xx = x0; xx <= x1; xx++) {
            gfx_putpixel(xx, y + yy, color);
        }
    }
}

#define abs(x) ((x) < 0 ? -(x) : (x))

void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        gfx_putpixel(x0, y0, color); // veya sende adı neyse (put_pixel vb.)
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
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

    /* unsupported/bad seq -> skip 1 byte */
    *ps += 1;
    return '?';
}

/* unicode -> senin KVX 0..255 charset */
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

    /* küçük buffer: satır satır çizmek için yeterli */
    char out[256];
    int oi = 0;

    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (!cp) break;

        uint8_t ch = unicode_to_kvx_byte(cp);

        /* newline gelirse flush et */
        if (ch == '\n' || ch == '\r') {
            out[oi] = '\0';
            if (oi) gfx_draw_text(x, y, color, out);
            oi = 0;
            y += 16; /* font yüksekliğin 16 ise */
            continue;
        }

        out[oi++] = (char)ch;

        if (oi >= (int)sizeof(out) - 1) {
            out[oi] = '\0';
            gfx_draw_text(x, y, color, out);
            oi = 0;
            /* aynı satırda devam edebilir, ama gerek yoksa bırak */
        }
    }

    out[oi] = '\0';
    if (oi) gfx_draw_text(x, y, color, out);
}

// Karekök fonksiyonu (Yuvarlak köşeler için yardımcı)
static uint32_t isqrt_u32(uint32_t n) {
    uint32_t x = n;
    uint32_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

// Modern UI için Yuvarlatılmış Köşeli Dikdörtgen (Filled)
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
                inset = r - (int)isqrt_u32(rr - dy * dy);
            } else if (yy >= h - r) {
                int dy = yy - (h - r);
                inset = r - (int)isqrt_u32(rr - dy * dy);
            }
        }

        int x0 = x + inset;
        int x1 = x + w - inset - 1;
        for (int xx = x0; xx <= x1; xx++) {
            gfx_putpixel(xx - g_origin_x, y + yy, color);
        }
    }
}

// İçi boş dikdörtgen kenarlığı çizimi
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    // Üst kenar
    gfx_draw_line(x, y, x + w - 1, y, color);
    // Alt kenar
    gfx_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    // Sol kenar
    gfx_draw_line(x, y, x, y + h - 1, color);
    // Sağ kenar
    gfx_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void gfx_set_origin(int x, int y) {
    g_origin_x = x;
    g_origin_y = y;
}

void gfx_reset_origin(void) {
    g_origin_x = 0;
    g_origin_y = 0;
}

typedef enum { CLIP_NONE=0, CLIP_RECT=1, CLIP_RRECT4=2 } clip_mode_t;

static clip_mode_t g_clip_mode = CLIP_NONE;

static int g_clip_x, g_clip_y, g_clip_w, g_clip_h;
static int g_clip_rtl, g_clip_rtr, g_clip_rbl, g_clip_rbr;

static inline int in_clip(int sx, int sy) {
    if (g_clip_mode == CLIP_NONE) return 1;

    if (sx < g_clip_x || sy < g_clip_y || sx >= g_clip_x + g_clip_w || sy >= g_clip_y + g_clip_h)
        return 0;

    if (g_clip_mode == CLIP_RECT) return 1;

    // rounded rect check
    int lx = sx - g_clip_x;
    int ly = sy - g_clip_y;
    int w  = g_clip_w;
    int h  = g_clip_h;

    // TL corner
    if (g_clip_rtl > 0 && lx < g_clip_rtl && ly < g_clip_rtl) {
        int r = g_clip_rtl;
        int dx = (r - 1) - lx;
        int dy = (r - 1) - ly;
        return (dx*dx + dy*dy) <= (r*r);
    }
    // TR
    if (g_clip_rtr > 0 && lx >= (w - g_clip_rtr) && ly < g_clip_rtr) {
        int r = g_clip_rtr;
        int dx = lx - (w - r);
        int dy = (r - 1) - ly;
        return (dx*dx + dy*dy) <= (r*r);
    }
    // BL
    if (g_clip_rbl > 0 && lx < g_clip_rbl && ly >= (h - g_clip_rbl)) {
        int r = g_clip_rbl;
        int dx = (r - 1) - lx;
        int dy = ly - (h - r);
        return (dx*dx + dy*dy) <= (r*r);
    }
    // BR
    if (g_clip_rbr > 0 && lx >= (w - g_clip_rbr) && ly >= (h - g_clip_rbr)) {
        int r = g_clip_rbr;
        int dx = lx - (w - r);
        int dy = ly - (h - r);
        return (dx*dx + dy*dy) <= (r*r);
    }

    return 1;
}

void gfx_blit_argb_key(int x, int y, int w, int h, const uint32_t* data, uint32_t key) {
    fb_blit_argb_key(x + g_origin_x, y + g_origin_y, w, h, data, key);
}

// UTF-8 -> CP1254 (Türkçe subset) tek byte çevirir.
// Dönen: 0..255 tek byte, '?' fallback
static uint8_t utf8_to_cp1254_1(const char** ps) {
    const unsigned char* s = (const unsigned char*)(*ps);

    if (s[0] < 0x80) { // ASCII
        (*ps)++;
        return (uint8_t)s[0];
    }

    // 2-byte sequences (Türkçe)
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        unsigned char b0 = s[0], b1 = s[1];
        (*ps) += 2;

        // UTF-8 -> Unicode codepoint
        uint16_t cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);

        // Türkçe harfleri CP1254'e map et
        switch (cp) {
            case 0x00E7: return 0xE7; // ç
            case 0x00C7: return 0xC7; // Ç
            case 0x011F: return 0xF0; // ğ  (sen 0xF0 kullanıyorsun)
            case 0x011E: return 0xD0; // Ğ  (fontta yoksa ekle)
            case 0x0131: return 0xFD; // ı
            case 0x0130: return 0xDD; // İ
            case 0x00F6: return 0xF6; // ö
            case 0x00D6: return 0xD6; // Ö
            case 0x00FC: return 0xFC; // ü
            case 0x00DC: return 0xDC; // Ü
            case 0x015F: return 0xFE; // ş
            case 0x015E: return 0xDE; // Ş
            default: return '?';
        }
    }

    // diğer UTF-8 uzunlukları: skip 1 byte fallback
    (*ps)++;
    return '?';
}

void gfx_clip_clear(void) {
    g_clip_mode = CLIP_NONE;
}

void gfx_clip_rect(int x, int y, int w, int h) {
    g_clip_mode = CLIP_RECT;
    g_clip_x = x; g_clip_y = y; g_clip_w = w; g_clip_h = h;
}

void gfx_clip_round_rect4(int x, int y, int w, int h, int rtl, int rtr, int rbl, int rbr) {
    g_clip_mode = CLIP_RRECT4;
    g_clip_x = x; g_clip_y = y; g_clip_w = w; g_clip_h = h;
    g_clip_rtl = rtl; g_clip_rtr = rtr; g_clip_rbl = rbl; g_clip_rbr = rbr;
}