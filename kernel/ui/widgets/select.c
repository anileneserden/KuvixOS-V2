#include <ui/widgets/select.h>
#include <stdint.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <font/font8x16_tr.h>

// ---------------- Helpers ----------------

static int pt_in(int x, int y, int w, int h, int px, int py) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static void draw_border_1px(int x, int y, int w, int h, fb_color_t c) {
    fb_draw_rect(x, y, w, 1, c);
    fb_draw_rect(x, y + h - 1, w, 1, c);
    fb_draw_rect(x, y, 1, h, c);
    fb_draw_rect(x + w - 1, y, 1, h, c);
}

// Küçük aşağı ok ikonu (v)
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

    // 8x16 font için satır yüksekliğini güvenli bir değere (24px) çekiyoruz
    s->item_h = (h > 24) ? h : 24;
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
    if (!s->open || s->count <= 0) { s->hover = -1; return; }

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
    (void)released; (void)buttons;
    if (!(pressed & 0x01)) return 0;

    // Ana kutuya tıklandıysa aç/kapat
    if (pt_in(s->x, s->y, s->w, s->h, mx, my)) {
        s->open = !s->open;
        s->has_focus = 1;
        return 1;
    }

    if (s->open) {
        int list_y = s->y + s->h;
        if (pt_in(s->x, list_y, s->w, s->count * s->item_h, mx, my)) {
            int idx = (my - list_y) / s->item_h;
            if (idx >= 0 && idx < s->count) {
                s->selected = idx;
                s->open = 0;
                s->hover = -1;
                return 1;
            }
        }
        s->open = 0;
        s->hover = -1;
        return 1;
    }

    s->has_focus = 0;
    return 0;
}

int ui_select_on_key(ui_select_t* s, uint16_t ev)
{
    if (!s->has_focus || s->count <= 0) return 0;
    uint8_t ch = (uint8_t)(ev & 0xFF);

    if (ch == 27) { s->open = 0; s->hover = -1; return 1; }
    if (ch == '\n') { s->open = !s->open; return 1; }
    
    if (ch == 'w' && s->selected > 0) { s->selected--; return 1; }
    if (ch == 's' && s->selected < s->count - 1) { s->selected++; return 1; }

    return 0;
}

void ui_select_draw(const ui_select_t* s)
{
    fb_color_t bg      = fb_rgb(245,245,245);
    fb_color_t border  = fb_rgb(120,120,120);
    fb_color_t text    = fb_rgb(0,0,0);
    fb_color_t hoverbg = fb_rgb(210,230,255);

    // Ana kutu çizimi
    fb_draw_rect(s->x, s->y, s->w, s->h, bg);
    draw_border_1px(s->x, s->y, s->w, s->h, border);

    // Seçili metni çiz (Dikey ortalama: s->h - 16 piksel font / 2)
    const char* label = (s->items && s->count > 0 && s->selected >= 0) ? s->items[s->selected] : "(none)";
    gfx_draw_text(s->x + 8, s->y + (s->h - 16) / 2, text, label);

    // Ok ikonu
    draw_arrow_down(s->x + s->w - 14, s->y + (s->h / 2) - 2, text);

    if (!s->open) return;

    // Dropdown listesi çizimi
    int list_y = s->y + s->h;
    fb_draw_rect(s->x, list_y, s->w, s->count * s->item_h, bg);
    draw_border_1px(s->x, list_y, s->w, s->count * s->item_h, border);

    for (int i = 0; i < s->count; i++) {
        int iy = list_y + i * s->item_h;
        if (i == s->hover) {
            fb_draw_rect(s->x + 1, iy, s->w - 2, s->item_h, hoverbg);
        }
        const char* it = s->items ? s->items[i] : "(null)";
        // Liste elemanlarını ortalayarak çiz
        gfx_draw_text(s->x + 8, iy + (s->item_h - 16) / 2, text, it);
    }
}

int ui_select_get_selected(const ui_select_t* s) { return s->selected; }

const char* ui_select_get_selected_text(const ui_select_t* s) {
    if (!s->items || s->selected < 0) return 0;
    return s->items[s->selected];
}