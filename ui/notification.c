#include <ui/notification.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

static char notify_text[64];
static int  notify_timer = 0;
static bool notify_visible = false;

#define NOTIFY_PAD_X   10
#define NOTIFY_PAD_Y   10
#define NOTIFY_H       45

#define FONT_W         8   // font8x16 varsayımı
#define FONT_H         16

#define MIN_W          180
#define MAX_W          520

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void trim_to_fit(char* s, int max_chars) {
    // max_chars: ekranda göstereceğimiz toplam karakter limiti (ASCII varsayımı)
    if (!s || max_chars <= 0) { if (s) s[0] = 0; return; }

    int n = (int)strlen(s);
    if (n <= max_chars) return;

    // "..." için 3 yer ayır
    if (max_chars <= 3) {
        s[0] = 0;
        return;
    }

    s[max_chars - 3] = '.';
    s[max_chars - 2] = '.';
    s[max_chars - 1] = '.';
    s[max_chars] = 0;
}

void notification_show(const char* text, uint32_t duration) {
    if (!text) text = "";
    strncpy(notify_text, text, sizeof(notify_text) - 1);
    notify_text[sizeof(notify_text) - 1] = 0;

    notify_timer = (int)duration;
    notify_visible = (notify_timer > 0);
}

void notification_draw(void) {
    if (!notify_visible || notify_timer <= 0) {
        notify_visible = false;
        return;
    }

    int screen_w = (int)fb_get_width();

    // prefix + text ölçümü (monospace)
    const char* prefix = "[!] ";
    int prefix_len = (int)strlen(prefix);
    int text_len   = (int)strlen(notify_text);

    // içerik genişliği: (prefix+text) * FONT_W
    int content_px = (prefix_len + text_len) * FONT_W;

    // kutu genişliği: padding + content + padding
    int w = NOTIFY_PAD_X + content_px + NOTIFY_PAD_X;

    // clamp
    w = clampi(w, MIN_W, MAX_W);

    // ekranın dışına taşmasın (sağdan 10px boşlukla)
    int max_w_screen = screen_w - 20;
    if (w > max_w_screen) w = max_w_screen;

    int h = NOTIFY_H;

    int x = screen_w - w - 10;
    int y = 10;

    // Eğer w clamp yüzünden küçüldüyse metni kırp
    // kullanılabilir içerik alanı:
    int inner_w = w - (NOTIFY_PAD_X * 2);
    int max_chars_total = inner_w / FONT_W;
    int max_text_chars  = max_chars_total - prefix_len;
    if (max_text_chars < 0) max_text_chars = 0;

    // notify_text'i geçici kopyaya alıp kırpalım (globali bozmayalım)
    char shown[64];
    strncpy(shown, notify_text, sizeof(shown) - 1);
    shown[sizeof(shown) - 1] = 0;
    trim_to_fit(shown, max_text_chars);

    // ARGB: 0xFFRRGGBB
    gfx_fill_rect(x, y, w, h, 0xFF1A1A1A);
    gfx_draw_rect(x, y, w, h, 0xFF00AAFF);

    // text baseline
    int ty = y + (h - FONT_H) / 2; // ortala

    gfx_draw_text_utf8(x + NOTIFY_PAD_X, ty, 0xFF00AAFF, prefix);
    gfx_draw_text_utf8(x + NOTIFY_PAD_X + (prefix_len * FONT_W), ty, 0xFFFFFFFF, shown);
}

void notification_tick(int delta_ms) {
    if (!notify_visible) return;

    if (delta_ms <= 0) delta_ms = 16; // default
    notify_timer -= delta_ms;

    if (notify_timer <= 0) {
        notify_timer = 0;
        notify_visible = false;
        notify_text[0] = 0;
    }
}

int notification_is_visible(void) {
    return notify_visible && notify_timer > 0;
}