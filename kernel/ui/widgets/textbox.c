#include <ui/widgets/textbox.h>
#include <kernel/drivers/input/input.h>
#include <ui/mouse.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/theme.h>
#include <lib/string.h>

extern uint32_t g_ticks_ms;

static int point_in_rect(int x, int y, int w, int h, int px, int py) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

void ui_textbox_init(ui_textbox_t* tb, int x, int y, int w, int h, char* buffer, int max_len) {
    const ui_theme_t* th = ui_get_theme();
    tb->x = x; tb->y = y; tb->w = w; tb->h = h;
    tb->buffer = buffer;
    tb->max_len = max_len;
    tb->len = 0;
    if (buffer && max_len > 0) buffer[0] = '\0';
    tb->has_focus = 0;
    tb->caret_pos = 0;
    tb->caret_visible = 1;
    tb->caret_last_toggle_ms = g_ticks_ms;
    tb->placeholder = 0;
    tb->text_color = th->textbox_text;
    tb->placeholder_color = th->textbox_placeholder;
    tb->bg_color = th->textbox_bg;
    tb->border_focus = th->textbox_focus_border;
    tb->border_nofocus = th->textbox_border;
    tb->use_password_char = 0;
    tb->password_char = '*';
}

void ui_textbox_set_placeholder(ui_textbox_t* tb, const char* text) { tb->placeholder = text; }
void ui_textbox_set_password_mode(ui_textbox_t* tb, int enabled) { tb->use_password_char = enabled ? 1 : 0; }
void ui_textbox_set_password_char(ui_textbox_t* tb, char c) { tb->password_char = c; }

void ui_textbox_update(ui_textbox_t* tb) {
    int left_now = (g_mouse.buttons & 0x1) != 0;
    int left_prev = (g_mouse.prev_buttons & 0x1) != 0;
    if (left_now && !left_prev) {
        if (point_in_rect(tb->x, tb->y, tb->w, tb->h, g_mouse.x, g_mouse.y)) {
            tb->has_focus = 1;
            tb->caret_pos = tb->len;
        } else tb->has_focus = 0;
    }
    uint32_t now = g_ticks_ms;
    if (now - tb->caret_last_toggle_ms >= 500) {
        tb->caret_last_toggle_ms = now;
        tb->caret_visible = !tb->caret_visible;
    }
}

void ui_textbox_draw(const ui_textbox_t* tb) {
    const ui_theme_t* th = ui_get_theme();
    uint32_t border_color = tb->has_focus ? tb->border_focus : tb->border_nofocus;

    gfx_fill_rect(tb->x, tb->y, tb->w, tb->h, tb->bg_color);
    
    // Kenarlık çizimi
    for (int i = 0; i < tb->w; i++) {
        gfx_putpixel(tb->x + i, tb->y, border_color);
        gfx_putpixel(tb->x + i, tb->y + tb->h - 1, border_color);
    }
    for (int i = 0; i < tb->h; i++) {
        gfx_putpixel(tb->x, tb->y + i, border_color);
        gfx_putpixel(tb->x + tb->w - 1, tb->y + i, border_color);
    }

    int text_x = tb->x + 4;
    int text_y = tb->y + (tb->h - 16) / 2; // 8x16 font ortalama

    if (tb->len == 0 && tb->placeholder && !tb->has_focus) {
        gfx_draw_text(text_x, text_y, tb->placeholder_color, tb->placeholder);
    } else {
        if (tb->use_password_char) {
            for (int i = 0; i < tb->len; i++) {
                int bx = text_x + i * 8 + 2;
                int by = text_y + 6;
                for(int dy=0; dy<4; dy++)
                    for(int dx=0; dx<4; dx++)
                        gfx_putpixel(bx + dx, by + dy, tb->text_color);
            }
        } else {
            gfx_draw_text(text_x, text_y, tb->text_color, tb->buffer);
        }
    }

    if (tb->has_focus && tb->caret_visible) {
        int cx = text_x + tb->caret_pos * 8;
        for (int i = 0; i < 16; i++) gfx_putpixel(cx, text_y + i, th->textbox_caret);
    }
}

void ui_textbox_on_mouse(ui_textbox_t* tb, int mx, int my, uint8_t pressed, uint8_t buttons) {
    (void)buttons; if (!(pressed & 0x1)) return;
    if (point_in_rect(tb->x, tb->y, tb->w, tb->h, mx, my)) {
        tb->has_focus = 1;
        tb->caret_pos = tb->len;
    } else tb->has_focus = 0;
}

void ui_textbox_on_key(ui_textbox_t* tb, uint16_t ev) {
    if (!tb->has_focus) return;
    if ((ev & 0xFF00) == 0xFF00) return;
    uint8_t ch = (uint8_t)(ev & 0xFF);
    int did_edit = 0;

    if (ch == '\b') {
        if (tb->len > 0 && tb->caret_pos > 0) {
            for (int i = tb->caret_pos - 1; i < tb->len - 1; i++) tb->buffer[i] = tb->buffer[i + 1];
            tb->len--; tb->caret_pos--; tb->buffer[tb->len] = '\0';
            did_edit = 1;
        }
    } else if (ch >= 32) {
        if (tb->len < tb->max_len - 1) {
            for (int i = tb->len; i > tb->caret_pos; i--) tb->buffer[i] = tb->buffer[i - 1];
            tb->buffer[tb->caret_pos] = (char)ch;
            tb->len++; tb->caret_pos++; tb->buffer[tb->len] = '\0';
            did_edit = 1;
        }
    }
    if (did_edit) { tb->caret_visible = 1; tb->caret_last_toggle_ms = g_ticks_ms; }
}