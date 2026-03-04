#include <ui/select/select.h>
#include <stdint.h>

#include <kernel/drivers/video/fb.h>
#include <font/font8x8_basic.h>

// ---------------- helpers ----------------
static int pt_in(int x, int y, int w, int h, int px, int py) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static uint8_t tr_from_utf8(const char* s, int* adv) {
    uint8_t c0 = (uint8_t)s[0];
    if (c0 < 0x80) { *adv = 1; return c0; } // ASCII

    uint8_t c1 = (uint8_t)s[1];

    // UTF-8 2-byte sequences for Turkish letters
    // Ç (U+00C7) = C3 87 -> 0xC7
    // ç (U+00E7) = C3 A7 -> 0xE7
    if (c0 == 0xC3) {
        *adv = 2;
        switch (c1) {
            case 0x87: return 0xC7; // Ç
            case 0xA7: return 0xE7; // ç
            case 0x96: return 0xD6; // Ö
            case 0xB6: return 0xF6; // ö
            case 0x9C: return 0xDC; // Ü
            case 0xBC: return 0xFC; // ü
            default:   return '?';
        }
    }

    // Ğ (U+011E) = C4 9E -> 0xD0 (sen fontta 0xD0 kullanmışsın)
    // ğ (U+011F) = C4 9F -> 0xF0
    // İ (U+0130) = C4 B0 -> (fonta eklemen lazım, aşağıda söyleyeceğim)
    // ı (U+0131) = C4 B1 -> (fonta eklemen lazım)
    if (c0 == 0xC4) {
        *adv = 2;
        switch (c1) {
            case 0x9E: return 0xD0; // Ğ  (not: normalde CP1254'te 0xD0 değil, ama sen öyle seçmişsin)
            case 0x9F: return 0xF0; // ğ
            case 0xB0: return 0xDD; // İ (öneri: fontta 0xDD indeksini İ yap)
            case 0xB1: return 0xFD; // ı (sen 0xFD'ye bir glyph koymuşsun, onu ı yapalım)
            default:   return '?';
        }
    }

    // Ş (U+015E) = C5 9E -> 0xDE
    // ş (U+015F) = C5 9F -> 0xFE
    if (c0 == 0xC5) {
        *adv = 2;
        switch (c1) {
            case 0x9E: return 0xDE; // Ş
            case 0x9F: return 0xFE; // ş
            default:   return '?';
        }
    }

    // bilinmeyen UTF-8: 1 byte ilerle, '?' bas
    *adv = 1;
    return '?';
}

static void draw_text8(int x, int y, fb_color_t color, const char* s) {
    while (*s) {
        int adv = 1;
        uint8_t c = tr_from_utf8(s, &adv);
        s += adv;

        const uint8_t* glyph = font8x8_basic[c];
        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) fb_putpixel(x + col, y + row, color);
            }
        }
        x += 8;
    }
}

static void draw_border_1px(int x, int y, int w, int h, fb_color_t c) {
    fb_draw_rect(x, y, w, 1, c);
    fb_draw_rect(x, y + h - 1, w, 1, c);
    fb_draw_rect(x, y, 1, h, c);
    fb_draw_rect(x + w - 1, y, 1, h, c);
}

// küçük aşağı ok (v) 5px genişlik
static void draw_arrow_down(int x, int y, fb_color_t c) {
    fb_putpixel(x + 0, y + 0, c);
    fb_putpixel(x + 1, y + 1, c);
    fb_putpixel(x + 2, y + 2, c);
    fb_putpixel(x + 3, y + 1, c);
    fb_putpixel(x + 4, y + 0, c);
}

// ---------------- API ----------------
void ui_select_init(ui_select_t* s, int x, int y, int w, int h,
                    const char** items, int count, int initial_index)
{
    s->x = x; s->y = y; s->w = w; s->h = h;
    s->items = items;
    s->count = count;

    if (initial_index < 0 || initial_index >= count) initial_index = 0;
    s->selected = (count > 0) ? initial_index : -1;

    s->open = 0;
    s->hover = -1;
    s->has_focus = 0;

    // dropdown satır yüksekliği
    s->item_h = (h > 0) ? h : 20;
}

void ui_select_set_items(ui_select_t* s, const char** items, int count, int initial_index)
{
    s->items = items;
    s->count = count;

    if (initial_index < 0 || initial_index >= count) initial_index = 0;
    s->selected = (count > 0) ? initial_index : -1;

    s->open = 0;
    s->hover = -1;
}

void ui_select_update(ui_select_t* s, int mx, int my)
{
    if (!s->open) { s->hover = -1; return; }
    if (s->count <= 0) { s->hover = -1; return; }

    int list_x = s->x;
    int list_y = s->y + s->h;
    int list_w = s->w;
    int list_h = s->count * s->item_h;

    if (!pt_in(list_x, list_y, list_w, list_h, mx, my)) {
        s->hover = -1;
        return;
    }

    int idx = (my - list_y) / s->item_h;
    if (idx < 0 || idx >= s->count) idx = -1;
    s->hover = idx;
}

int ui_select_on_mouse(ui_select_t* s, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)released;
    (void)buttons;

    // sol click press değilse yok
    if (!(pressed & 0x01)) return 0;

    // 1) kutuya tıklandıysa: toggle
    if (pt_in(s->x, s->y, s->w, s->h, mx, my)) {
        s->open = !s->open;
        s->has_focus = 1;
        return 1;
    }

    // 2) açıksa listede seçme
    if (s->open) {
        int list_x = s->x;
        int list_y = s->y + s->h;
        int list_w = s->w;
        int list_h = s->count * s->item_h;

        if (pt_in(list_x, list_y, list_w, list_h, mx, my)) {
            int idx = (my - list_y) / s->item_h;
            if (idx >= 0 && idx < s->count) {
                int changed = (idx != s->selected);
                s->selected = idx;
                s->open = 0;
                s->hover = -1;
                return changed ? 1 : 1; // event tüketildi
            }
        }

        // 3) dışarı tık: kapat
        s->open = 0;
        s->hover = -1;
        return 1;
    }

    // 4) kapalıyken başka yere tık: focus kapat
    s->has_focus = 0;
    return 0;
}

int ui_select_on_key(ui_select_t* s, uint16_t ev)
{
    if (!s->has_focus) return 0;
    if (s->count <= 0) return 0;

    uint8_t ch = (uint8_t)(ev & 0xFF);

    // ESC
    if (ch == 27) {
        s->open = 0;
        s->hover = -1;
        return 1;
    }

    // ENTER
    if (ch == '\n') {
        s->open = !s->open;
        s->hover = -1;
        return 1;
    }

    // Şimdilik test: w/s
    if (ch == 'w') {
        if (s->selected > 0) s->selected--;
        return 1;
    }
    if (ch == 's') {
        if (s->selected < s->count - 1) s->selected++;
        return 1;
    }

    return 0;
}

void ui_select_draw(const ui_select_t* s)
{
    // Şimdilik theme bağlamadan sabit renkler (sonra th->select_* yaparız)
    fb_color_t bg      = fb_rgb(245,245,245);
    fb_color_t border  = fb_rgb(120,120,120);
    fb_color_t text    = fb_rgb(0,0,0);
    fb_color_t hoverbg = fb_rgb(210,230,255);

    // main box
    fb_draw_rect(s->x, s->y, s->w, s->h, bg);
    draw_border_1px(s->x, s->y, s->w, s->h, border);

    // selected text
    const char* label = "(none)";
    if (s->items && s->count > 0 && s->selected >= 0 && s->selected < s->count) {
        label = s->items[s->selected];
    }

    int ty = s->y + (s->h - 8) / 2; // 8px font ortalama
    draw_text8(s->x + 8, ty, text, label);

    // arrow
    int ax = s->x + s->w - 14;
    int ay = s->y + (s->h / 2) - 2;
    draw_arrow_down(ax, ay, text);

    if (!s->open) return;

    // dropdown list
    int list_x = s->x;
    int list_y = s->y + s->h;
    int list_w = s->w;
    int list_h = s->count * s->item_h;

    fb_draw_rect(list_x, list_y, list_w, list_h, bg);
    draw_border_1px(list_x, list_y, list_w, list_h, border);

    for (int i = 0; i < s->count; i++) {
        int iy = list_y + i * s->item_h;

        if (i == s->hover) {
            fb_draw_rect(list_x + 1, iy, list_w - 2, s->item_h, hoverbg);
        }

        const char* it = s->items ? s->items[i] : "(null)";
        draw_text8(list_x + 8, iy + (s->item_h - 8) / 2, text, it);
    }
}

int ui_select_get_selected(const ui_select_t* s)
{
    return s->selected;
}

const char* ui_select_get_selected_text(const ui_select_t* s)
{
    if (!s->items || s->count <= 0) return 0;
    if (s->selected < 0 || s->selected >= s->count) return 0;
    return s->items[s->selected];
}
