// src/lib/ui/textbox/textbox.c
#include <ui/widgets/textbox.h>

#include <kernel/drivers/input/input.h>
#include <ui/mouse.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/drivers/video/fb.h>
#include <font/font8x8_basic.h>
#include <ui/theme.h>

extern uint32_t g_ticks_ms;

// Küçük helper: nokta dikdörtgen içinde mi?
static int point_in_rect(int x, int y, int w, int h, int px, int py) {
    return (px >= x && px < x + w &&
            py >= y && py < y + h);
}

static void draw_text(int x, int y, uint32_t color, const char* s)
{
    while (*s) {
        uint8_t c = (uint8_t)*s;
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

void ui_textbox_init(ui_textbox_t* tb,
                     int x, int y, int w, int h,
                     char* buffer, int max_len)
{
    const ui_theme_t* th = ui_get_theme();

    tb->x = x;
    tb->y = y;
    tb->w = w;
    tb->h = h;

    tb->buffer  = buffer;
    tb->max_len = max_len;
    tb->len     = 0;
    if (buffer && max_len > 0) buffer[0] = '\0';

    tb->has_focus = 0;

    tb->caret_pos            = 0;
    tb->caret_visible        = 1;
    tb->caret_last_toggle_ms = g_ticks_ms;

    tb->placeholder = 0;

    // Default renkler theme’den
    tb->text_color         = th->textbox_text;
    tb->placeholder_color  = th->textbox_placeholder;
    tb->bg_color           = th->textbox_bg;
    tb->border_focus       = th->textbox_focus_border;
    tb->border_nofocus     = th->textbox_border;

    tb->use_password_char = 0;
    tb->password_char     = '*';
}

void ui_textbox_set_placeholder(ui_textbox_t* tb, const char* text)
{
    tb->placeholder = text;
}

void ui_textbox_set_password_mode(ui_textbox_t* tb, int enabled)
{
    tb->use_password_char = enabled ? 1 : 0;
}

void ui_textbox_set_password_char(ui_textbox_t* tb, char c)
{
    tb->password_char = c;
}

void ui_textbox_set_colors(ui_textbox_t* tb,
                           uint32_t text_color,
                           uint32_t placeholder_color,
                           uint32_t bg_color,
                           uint32_t border_focus,
                           uint32_t border_nofocus)
{
    tb->text_color        = text_color;
    tb->placeholder_color = placeholder_color;
    tb->bg_color          = bg_color;
    tb->border_focus      = border_focus;
    tb->border_nofocus    = border_nofocus;
}

void ui_textbox_update(ui_textbox_t* tb)
{
    int left_now  = (g_mouse.buttons      & 0x1) != 0;
    int left_prev = (g_mouse.prev_buttons & 0x1) != 0;

    if (left_now && !left_prev) {
        if (point_in_rect(tb->x, tb->y, tb->w, tb->h, g_mouse.x, g_mouse.y)) {
            tb->has_focus = 1;
            tb->caret_pos = tb->len;
        } else {
            tb->has_focus = 0;
        }
    }

    uint32_t now = g_ticks_ms;
    if (now - tb->caret_last_toggle_ms >= 500) {
        tb->caret_last_toggle_ms = now;
        tb->caret_visible = !tb->caret_visible;
    }

    if (!tb->has_focus)
        return;
}

void ui_textbox_draw(const ui_textbox_t* tb)
{
    const ui_theme_t* th = ui_get_theme();

    uint32_t border_color = tb->has_focus ? tb->border_focus : tb->border_nofocus;

    // Arka plan
    for (int y = 0; y < tb->h; y++) {
        for (int x = 0; x < tb->w; x++) {
            fb_putpixel(tb->x + x, tb->y + y, tb->bg_color);
        }
    }

    // Kenarlık
    for (int x = 0; x < tb->w; x++) {
        fb_putpixel(tb->x + x, tb->y,              border_color);
        fb_putpixel(tb->x + x, tb->y + tb->h - 1, border_color);
    }
    for (int y = 0; y < tb->h; y++) {
        fb_putpixel(tb->x,             tb->y + y, border_color);
        fb_putpixel(tb->x + tb->w - 1, tb->y + y, border_color);
    }

    int text_x = tb->x + 4;
    int text_y = tb->y + (tb->h - 8) / 2;

    if (tb->len == 0 && tb->placeholder && !tb->has_focus) {
        draw_text(text_x, text_y, tb->placeholder_color, tb->placeholder);
    } else {
        if (tb->use_password_char) {
            int n = tb->len;
            for (int i = 0; i < n; i++) {
                int base_x = text_x + i * 8 + 3;
                int base_y = text_y + 2;

                for (int dy = 0; dy < 4; dy++) {
                    for (int dx = 0; dx < 4; dx++) {
                        fb_putpixel(base_x + dx, base_y + dy, tb->text_color);
                    }
                }
            }
        } else {
            draw_text(text_x, text_y, tb->text_color, tb->buffer);
        }
    }

    // Caret: theme caret rengi
    if (tb->has_focus && tb->caret_visible) {
        int caret_x = text_x + tb->caret_pos * 8;
        int caret_y = tb->y + 3;

        for (int y = 0; y < tb->h - 6; y++) {
            fb_putpixel(caret_x, caret_y + y, th->textbox_caret);
        }
    }
}

void ui_textbox_on_mouse(ui_textbox_t* tb, int mx, int my, uint8_t pressed, uint8_t buttons)
{
    (void)buttons;
    if (!(pressed & 0x1)) return;

    if (point_in_rect(tb->x, tb->y, tb->w, tb->h, mx, my)) {
        tb->has_focus = 1;
        tb->caret_pos = tb->len;
    } else {
        tb->has_focus = 0;
    }
}

void ui_textbox_on_key(ui_textbox_t* tb, uint16_t ev)
{
    if (!tb->has_focus) return;

    if ((ev & 0xFF00) == 0xFF00) return;

    uint8_t ch = (uint8_t)(ev & 0xFF);
    int did_edit = 0;

    if (ch == '\b') {
        if (tb->len > 0 && tb->caret_pos > 0) {
            for (int i = tb->caret_pos - 1; i < tb->len - 1; i++)
                tb->buffer[i] = tb->buffer[i + 1];
            tb->len--;
            tb->caret_pos--;
            tb->buffer[tb->len] = '\0';
            did_edit = 1;
        }
    } else if (ch == '\n' || ch == '\r') {
        // ignore
    } else if (ch >= 32) {
        if (tb->len < tb->max_len - 1) {
            for (int i = tb->len; i > tb->caret_pos; i--)
                tb->buffer[i] = tb->buffer[i - 1];
            tb->buffer[tb->caret_pos] = (char)ch;
            tb->len++;
            tb->caret_pos++;
            tb->buffer[tb->len] = '\0';
            did_edit = 1;
        }
    }

    if (did_edit) {
        tb->caret_visible = 1;
        tb->caret_last_toggle_ms = g_ticks_ms;
    }
}
