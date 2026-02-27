#include <ui/controls/textbox2.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

extern char kbd_scancode_to_ascii(uint8_t sc);

static textbox2_t* TB(ui_control_t* c) {
    return (textbox2_t*)c;
}

static void textbox2_draw(ui_control_t* c) {
    textbox2_t* tb = TB(c);
    if (!tb->base.visible) return;

    int x = tb->base.location.x;
    int y = tb->base.location.y;
    int w = tb->base.size.w;
    int h = tb->base.size.h;

    uint32_t bg = 0x101010;
    uint32_t bd = tb->focused ? 0xFF6A00 : 0x505050;

    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, bd);

    int pad = 6;
    int tx = x + pad;
    int ty = y + (h - 8) / 2;

    if (tb->len == 0 && tb->hint) {
        gfx_draw_text_utf8(tx, ty, 0x808080, tb->hint);
    } else {
        gfx_draw_text_utf8(tx, ty, 0xFFFFFF, tb->text);
    }

    if (tb->focused) {
        int cx = tx + tb->caret * 8;
        gfx_draw_text_utf8(cx, ty, 0xFF6A00, "_");
    }
}

static void insert_char(textbox2_t* tb, char c) {
    if (tb->readonly) return;
    if (tb->len >= TEXTBOX2_MAX - 1) return;
    if (tb->caret < 0) tb->caret = 0;
    if (tb->caret > tb->len) tb->caret = tb->len;

    for (int i = tb->len; i >= tb->caret; i--) {
        tb->text[i + 1] = tb->text[i];
    }

    tb->text[tb->caret] = c;
    tb->len++;
    tb->caret++;
    tb->text[tb->len] = 0;

    if (tb->on_change) tb->on_change(tb);
}

static void backspace(textbox2_t* tb) {
    if (tb->readonly) return;
    if (tb->caret <= 0) return;

    for (int i = tb->caret - 1; i < tb->len; i++) {
        tb->text[i] = tb->text[i + 1];
    }

    tb->caret--;
    tb->len--;
    if (tb->len < 0) tb->len = 0;
    tb->text[tb->len] = 0;

    if (tb->on_change) tb->on_change(tb);
}

static bool textbox2_handle(ui_control_t* c, const ui_event_t* e) {
    textbox2_t* tb = TB(c);

    switch (e->type) {
        case UI_EVT_MOUSE_DOWN:
            if (ui_control_contains(c, e->mouse_x, e->mouse_y)) {
                tb->focused = true;
                return true;
            } else {
                tb->focused = false;
            }
            break;

        case UI_EVT_KEY_DOWN:
            if (!tb->focused) break;

            {
                uint8_t sc = (uint8_t)(e->key & 0xFF);

                // break (release) ignore
                if (sc & 0x80) return false;

                // Enter
                if (sc == 0x1C) {
                    if (tb->on_enter) tb->on_enter(tb);
                    return true;
                }

                char ch = kbd_scancode_to_ascii(sc);

                // Backspace (ASCII 8)
                if (ch == '\b') {
                    backspace(tb);
                    return true;
                }

                // printable
                if (ch >= 32 && ch <= 126) {
                    insert_char(tb, ch);
                    return true;
                }
            }
            break;

        default:
            break;
    }

    return false;
}

static const ui_control_vtbl_t textbox2_vtbl = {
    .draw = textbox2_draw,
    .handle_event = textbox2_handle,
};

void textbox2_init(textbox2_t* tb, int id, ui_point_t loc, ui_size_t size) {
    memset(tb, 0, sizeof(*tb));

    ui_control_init(&tb->base, id, loc, size, &textbox2_vtbl);

    tb->hint = "type...";
    tb->text[0] = 0;
    tb->len = 0;
    tb->caret = 0;
    tb->focused = false;
    tb->readonly = false;
}

const char* textbox2_get_text(textbox2_t* tb) {
    return tb ? tb->text : "";
}

void textbox2_set_text(textbox2_t* tb, const char* s) {
    if (!tb) return;
    strncpy(tb->text, s ? s : "", TEXTBOX2_MAX - 1);
    tb->text[TEXTBOX2_MAX - 1] = 0;
    tb->len = (int)strlen(tb->text);
    tb->caret = tb->len;
}