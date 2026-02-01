// src/lib/ui/ui_button/ui_button.c
#include <ui/ui_button/ui_button.h>
#include <ui/mouse.h>
#include <kernel/drivers/video/fb.h>
#include <font/font8x8_basic.h>
#include <ui/theme.h>

// Basit nokta-dikdörtgen testi
static int point_in_rect(int x, int y, int w, int h, int px, int py) {
    return (px >= x && px < x + w &&
            py >= y && py < y + h);
}

static void draw_text(int x, int y, uint32_t color, const char* s)
{
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c > 127) c = '?';

        const uint8_t* glyph = font8x8_basic[c];

        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1u << (7 - col))) {
                    fb_putpixel(x + col, y + row, color);
                }
            }
        }

        x += 8;
        s++;
    }
}

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

    // Label'i ortala
    if (btn->label) {
        int len = 0;
        const char* p = btn->label;
        while (*p++) len++;

        int text_w = len * 8;
        int text_h = 8;

        int text_x = btn->x + (btn->w - text_w) / 2;
        int text_y = btn->y + (btn->h - text_h) / 2;

        draw_text(text_x, text_y, text_color, btn->label);
    }
}
