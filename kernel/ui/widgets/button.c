// src/lib/ui/ui_button/ui_button.c
#include <ui/widgets/button.h>
#include <ui/mouse.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h> // Merkezi çizim için şart
#include <ui/theme.h>
#include <lib/string.h>               // strlen kullanımı için

// Basit nokta-dikdörtgen testi
static int point_in_rect(int x, int y, int w, int h, int px, int py) {
    return (px >= x && px < x + w &&
            py >= y && py < y + h);
}

/* SİLDİK: static void draw_text(...) 
   Artık merkezi gfx_draw_text kullanıyoruz. 
*/

void ui_button_init(ui_button_t* btn,
                    int x, int y, int w, int h,
                    const char* label)
{
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->label = label;
    btn->is_hover   = 0;
    btn->is_pressed = 0;
}

int ui_button_update(ui_button_t* btn)
{
    int left_now  = (g_mouse.buttons      & 0x1) != 0;
    int left_prev = (g_mouse.prev_buttons & 0x1) != 0;

    int inside = point_in_rect(btn->x, btn->y, btn->w, btn->h,
                               g_mouse.x, g_mouse.y);

    btn->is_hover = inside;
    int clicked = 0;

    if (inside && left_now && !left_prev) {
        btn->is_pressed = 1;
    }

    if (!left_now && left_prev) {
        if (btn->is_pressed && inside) {
            clicked = 1;
        }
        btn->is_pressed = 0;
    }

    return clicked;
}

void ui_button_draw(const ui_button_t* btn)
{
    const ui_theme_t* th = ui_get_theme();

    uint32_t bg;
    uint32_t border;
    uint32_t text_color = th->button_text;

    if (btn->is_pressed) {
        bg     = th->button_pressed_bg;
        border = th->button_pressed_border;
    } else if (btn->is_hover) {
        bg     = th->button_hover_bg;
        border = th->button_hover_border;
    } else {
        bg     = th->button_bg;
        border = th->button_border;
    }

    // Gövde doldur
    for (int y = 0; y < btn->h; y++) {
        for (int x = 0; x < btn->w; x++) {
            fb_putpixel(btn->x + x, btn->y + y, bg);
        }
    }

    // Kenarlık
    for (int x = 0; x < btn->w; x++) {
        fb_putpixel(btn->x + x, btn->y,              border);
        fb_putpixel(btn->x + x, btn->y + btn->h - 1, border);
    }
    for (int y = 0; y < btn->h; y++) {
        fb_putpixel(btn->x,              btn->y + y, border);
        fb_putpixel(btn->x + btn->w - 1, btn->y + y, border);
    }

    // Label'i ortala (Yeni 8x16 ölçülerine göre)
    if (btn->label) {
        int len = strlen(btn->label);

        int text_w = len * 8;
        int text_h = 16; // Yeni yükseklik 16!

        int text_x = btn->x + (btn->w - text_w) / 2;
        int text_y = btn->y + (btn->h - text_h) / 2;

        // Merkezi fonksiyonu çağırıyoruz (Türkçe karakterler artık '?' olmayacak)
        gfx_draw_text(text_x, text_y, text_color, btn->label);
    }
}