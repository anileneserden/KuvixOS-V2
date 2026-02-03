#include <ui/widgets/label.h>
#include <kernel/drivers/video/fb.h>
#include <font/font8x8_basic.h>

static void draw_text8(int x, int y, uint32_t color, const char* s)
{
    while (*s) {
        uint8_t c = (uint8_t)*s++;
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
    }
}

void ui_label_init(ui_label_t* l, int x, int y, const char* text, uint32_t color)
{
    l->x = x; l->y = y; l->text = text; l->color = color;
}

void ui_label_set_text(ui_label_t* l, const char* text)
{
    l->text = text;
}

void ui_label_draw(const ui_label_t* l)
{
    if (!l || !l->text) return;
    draw_text8(l->x, l->y, l->color, l->text);
}
