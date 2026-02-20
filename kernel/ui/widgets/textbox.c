#include <ui/widgets/textbox.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

extern uint32_t g_ticks_ms;

static bool hit_rect(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

bool ui_textbox_hit(ui_textbox_t* tb, int mx, int my) {
    if (!tb) return false;
    return hit_rect(mx, my, tb->x, tb->y, tb->w, tb->h);
}

void ui_textbox_init(ui_textbox_t* tb, int x, int y, int w, int h, char* buffer, int capacity) {
    if (!tb) return;
    memset(tb, 0, sizeof(*tb));

    tb->x = x; tb->y = y; tb->w = w; tb->h = h;

    tb->buf = buffer;
    tb->cap = capacity;
    tb->len = 0;
    tb->caret = 0;

    if (tb->buf && tb->cap > 0) {
        tb->buf[0] = '\0';
    }

    tb->focused = false;
    tb->hovered = false;

    tb->last_blink_ms = g_ticks_ms;
    tb->caret_on = true;

    // default style
    tb->bg = 0xFFFFFF;
    tb->fg = 0x000000;
    tb->border = 0x808080;
    tb->border_focus = 0x0000AA;
}

static void tb_insert(ui_textbox_t* tb, char c) {
    if (!tb || !tb->buf) return;
    if (tb->len >= tb->cap - 1) return;

    // shift right
    for (int i = tb->len; i > tb->caret; i--) {
        tb->buf[i] = tb->buf[i - 1];
    }
    tb->buf[tb->caret] = c;
    tb->len++;
    tb->caret++;
    tb->buf[tb->len] = '\0';
}

static void tb_backspace(ui_textbox_t* tb) {
    if (!tb || !tb->buf) return;
    if (tb->caret <= 0 || tb->len <= 0) return;

    // remove at caret-1
    for (int i = tb->caret - 1; i < tb->len - 1; i++) {
        tb->buf[i] = tb->buf[i + 1];
    }
    tb->len--;
    tb->caret--;
    tb->buf[tb->len] = '\0';
}

static void tb_delete(ui_textbox_t* tb) {
    if (!tb || !tb->buf) return;
    if (tb->caret >= tb->len) return;

    for (int i = tb->caret; i < tb->len - 1; i++) {
        tb->buf[i] = tb->buf[i + 1];
    }
    tb->len--;
    tb->buf[tb->len] = '\0';
}

static void tb_move_left(ui_textbox_t* tb) {
    if (tb->caret > 0) tb->caret--;
}

static void tb_move_right(ui_textbox_t* tb) {
    if (tb->caret < tb->len) tb->caret++;
}

void ui_textbox_mouse(ui_textbox_t* tb, int mx, int my, uint8_t pressed, uint8_t released) {
    (void)released;
    if (!tb) return;

    tb->hovered = ui_textbox_hit(tb, mx, my);

    if (pressed & 1) {
        if (tb->hovered) {
            tb->focused = true;
            tb->caret_on = true;
            tb->last_blink_ms = g_ticks_ms;

            // Basit caret yerleşimi: tık = sona taşı
            tb->caret = tb->len;
        } else {
            tb->focused = false;
        }
    }
}

void ui_textbox_key(ui_textbox_t* tb, uint16_t scancode, char ascii) {
    if (!tb || !tb->focused) return;

    uint8_t sc = (uint8_t)(scancode & 0xFF);

    // ignore break
    if (sc & 0x80) return;

    // Enter -> ignore (Run app decide)
    if (sc == 0x1C) return;

    // Backspace (ascii '\b' geliyor sende)
    if (ascii == '\b') {
        tb_backspace(tb);
        return;
    }

    // Delete (Set1: 0x53) ama E0 olabilir; şimdilik sadece normal 0x53
    if (sc == 0x53) {
        tb_delete(tb);
        return;
    }

    // Arrows (Set1): Left 0x4B Right 0x4D
    if (sc == 0x4B) { tb_move_left(tb); return; }
    if (sc == 0x4D) { tb_move_right(tb); return; }

    // printable
    if (ascii >= 32 && ascii <= 126) {
        tb_insert(tb, ascii);
        return;
    }
}

void ui_textbox_draw(ui_textbox_t* tb) {
    if (!tb) return;

    // caret blink
    if (tb->focused) {
        if ((g_ticks_ms - tb->last_blink_ms) > 450) {
            tb->caret_on = !tb->caret_on;
            tb->last_blink_ms = g_ticks_ms;
        }
    } else {
        tb->caret_on = false;
    }

    // background + border
    gfx_fill_rect(tb->x, tb->y, tb->w, tb->h, tb->bg);
    gfx_draw_rect(tb->x, tb->y, tb->w, tb->h, tb->focused ? tb->border_focus : tb->border);

    // text (simple, single-line)
    int tx = tb->x + 6;
    int ty = tb->y + 4;
    if (tb->buf) {
        gfx_draw_text_utf8(tx, ty, tb->fg, tb->buf);
    }

    // caret (underscore style)
    if (tb->focused && tb->caret_on) {
        // monospace 8px varsayımı: caret x = tx + caret*8
        int cx = tx + tb->caret * 8;
        int cy = ty + 12;
        gfx_draw_line(cx, cy, cx + 6, cy, tb->fg);
    }
}