#include <ui/debug_overlay.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>

static bool g_enabled = true; // başlangıçta açık istersen true

void debug_overlay_set_enabled(bool en) { g_enabled = en; }
bool debug_overlay_is_enabled(void) { return g_enabled; }

static void itoa_simple(int v, char* out) {
    char tmp[16];
    int i = 0, neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    if (v == 0) tmp[i++] = '0';
    while (v > 0 && i < 15) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
    int p = 0;
    if (neg) out[p++] = '-';
    while (i > 0) out[p++] = tmp[--i];
    out[p] = 0;
}

static void draw_kv(int x, int y, const char* k, int v) {
    char n[16]; itoa_simple(v, n);
    gfx_draw_text_utf8(x, y, 0x00FFFFFF, k);
    gfx_draw_text_utf8(x + 90, y, 0x00FFFF00, n);
}

static void draw_kv_hex8(int x, int y, const char* k, uint8_t v) {
    char h[5];
    const char* d = "0123456789ABCDEF";
    h[0] = '0'; h[1] = 'x';
    h[2] = d[(v >> 4) & 0xF];
    h[3] = d[(v >> 0) & 0xF];
    h[4] = 0;
    gfx_draw_text_utf8(x, y, 0x00FFFFFF, k);
    gfx_draw_text_utf8(x + 90, y, 0x00FFFF00, h);
}

void debug_overlay_draw(int mx, int my, int dx, int dy,
                        int wheel_step, int wheel_total, uint8_t buttons) {
    if (!g_enabled) return;

    // Sol üstte küçük panel
    int x = 8, y = 8;
    int w = 210, h = 110;

    // Arkayı hafif kapat (solid)
    gfx_fill_rect(x - 4, y - 4, w, h, 0x00202020);
    gfx_draw_rect(x - 4, y - 4, w, h, 0x00AAAAAA);

    gfx_draw_text_utf8(x, y, 0x00FFFFFF, "DEBUG INPUT");
    y += 16;

    draw_kv(x, y, "mx", mx); y += 14;
    draw_kv(x, y, "my", my); y += 14;
    draw_kv(x, y, "dx", dx); y += 14;
    draw_kv(x, y, "dy", dy); y += 14;
    draw_kv(x, y, "wheel", wheel_step); y += 14;
    draw_kv(x, y, "w_total", wheel_total); y += 14;
    draw_kv_hex8(x, y, "buttons", buttons);
}