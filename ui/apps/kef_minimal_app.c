#include <ui/apps/kef_minimal_app.h>

#include <kernel/drivers/input/keyboard.h>
#include <kernel/exec/kef_json.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <ui/wm.h>
#include <ui/color.h>

kef_widget_t* kef_get_widget_ptr(kef_minimal_state_t* st, const char* id) {
    if (!st || !id) return 0;

    for (int i = 0; i < st->widget_count; i++) {
        if (strcmp(st->widgets[i].id, id) == 0) {
            return &st->widgets[i];
        }
    }

    return 0;
}

void kef_set_text(kef_minimal_state_t* st, const char* id, const char* text) {
    if (!st || !id || !text) return;

    kef_widget_t* w = kef_get_widget_ptr(st, id);
    if (!w) return;

    strncpy(w->text, text, sizeof(w->text) - 1);
    w->text[sizeof(w->text) - 1] = 0;

    wm_invalidate_window(st->window_id);
}

static int point_in_widget(kef_widget_t* w, int mx, int my) {
    if (!w) return 0;

    return (mx >= w->x && my >= w->y &&
            mx < (w->x + w->w) &&
            my < (w->y + w->h));
}

static void on_ok_click(kef_widget_t* self) {
    if (!self || !self->owner) return;

    kef_minimal_state_t* st = self->owner;
    kef_widget_t* input = kef_get_widget_ptr(st, "nameInput");

    if (input && input->value[0]) {
        kef_set_text(st, "statusLabel", input->value);
    } else {
        kef_set_text(st, "statusLabel", "Durum: Bos");
    }
}

static void kef_bind_events(kef_minimal_state_t* st) {
    if (!st) return;

    kef_widget_t* btn = kef_get_widget_ptr(st, "okButton");
    if (btn) {
        btn->on_click = on_ok_click;
    }
}

/*
 * keyev masaüstünden ham scancode olarak geliyor.
 * Karakter üretmek için mevcut keyboard layout helper'ını kullanıyoruz.
 */
static int keyev_to_char(uint16_t keyev) {
    uint8_t sc8 = (uint8_t)(keyev & 0xFF);

    /* break code ise karakter üretme */
    if (sc8 & 0x80) return 0;

    char c = kbd_scancode_to_ascii(sc8);
    if (!c) return 0;

    return (int)(unsigned char)c;
}

static int keyev_is_backspace(uint16_t keyev) {
    uint8_t sc8 = (uint8_t)(keyev & 0xFF);

    /* Set1 backspace make code */
    return sc8 == 0x0E;
}

static void kef_minimal_on_create(app_t* self) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st) return;

    memset(st, 0, sizeof(*st));
    st->window_id = self->win_id;

    if (kef_json_load_file("/apps/hello.json", st)) {
        st->loaded = 1;
        wm_set_title(self->win_id, st->title);
        wm_set_window_size(self->win_id, st->width, st->height);
        kef_bind_events(st);

        printk("[KEFJSON] app loaded ok (win=%d size=%dx%d)\n",
            self->win_id, st->width, st->height);
    } else {
        st->loaded = 0;
        strcpy(st->title, "KEF JSON");
        st->width = 420;
        st->height = 240;
        st->bg_color = 0xE6E6E6;
        wm_set_title(self->win_id, st->title);
    }

    wm_invalidate_window(self->win_id);
}

static void kef_minimal_on_draw(app_t* self) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st) return;

    ui_rect_t c = wm_get_client_rect(st->window_id);
    gfx_fill_rect(0, 0, c.w, c.h, st->bg_color);

    if (!st->loaded) {
        gfx_draw_text_utf8(12, 12, COLOR_BLACK, "hello.json yuklenemedi");
        return;
    }

    for (int i = 0; i < st->widget_count; i++) {
        kef_widget_t* w = &st->widgets[i];

        if (w->type == KEF_WIDGET_LABEL) {
            gfx_draw_text_utf8(w->x, w->y, w->text_color, w->text);
        }
        else if (w->type == KEF_WIDGET_BUTTON) {
            uint32_t bg = w->pressed ? 0xB0B0B0 : 0xD8D8D8;

            gfx_fill_rect(w->x, w->y, w->w, w->h, bg);
            gfx_draw_rect(w->x, w->y, w->w, w->h, COLOR_BLACK);
            gfx_draw_text_utf8(w->x + 8, w->y + 8, w->text_color, w->text);
        }
        else if (w->type == KEF_WIDGET_INPUT) {
            gfx_fill_rect(w->x, w->y, w->w, w->h, 0xFFFFFF);
            gfx_draw_rect(w->x, w->y, w->w, w->h, w->focused ? 0x00A2FF : COLOR_BLACK);

            if (w->value[0]) {
                /* input buffer tek-byte KVX charset */
                gfx_draw_text(w->x + 4, w->y + 6, w->text_color, w->value);
            } else {
                /* placeholder JSON'dan geliyor, UTF-8 */
                gfx_draw_text_utf8(w->x + 4, w->y + 6, 0x888888, w->placeholder);
            }
        }
    }
}

static void kef_minimal_on_mouse(app_t* self, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn) {
    (void)btn;

    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st || !st->loaded) return;

    if (pr & 0x01) {
        /* önce tüm input focuslarını kapat */
        for (int i = 0; i < st->widget_count; i++) {
            kef_widget_t* w = &st->widgets[i];
            if (w->type == KEF_WIDGET_INPUT) {
                w->focused = 0;
            }
        }

        /* tıklanan input varsa focus ver */
        for (int i = 0; i < st->widget_count; i++) {
            kef_widget_t* w = &st->widgets[i];

            if (w->type == KEF_WIDGET_INPUT && point_in_widget(w, mx, my)) {
                w->focused = 1;
                wm_invalidate_window(st->window_id);
            }
        }
    }

    for (int i = 0; i < st->widget_count; i++) {
        kef_widget_t* w = &st->widgets[i];

        if (w->type != KEF_WIDGET_BUTTON) continue;

        if (pr & 0x01) {
            if (point_in_widget(w, mx, my)) {
                w->pressed = 1;
                wm_invalidate_window(st->window_id);
            }
        }

        if (rel & 0x01) {
            int was_pressed = w->pressed;
            w->pressed = 0;
            wm_invalidate_window(st->window_id);

            if (was_pressed && point_in_widget(w, mx, my)) {
                if (w->on_click) {
                    w->on_click(w);
                }
            }
        }
    }
}

static void kef_minimal_on_key(app_t* self, uint16_t keyev) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st || !st->loaded) return;

    for (int i = 0; i < st->widget_count; i++) {
        kef_widget_t* w = &st->widgets[i];
        if (w->type != KEF_WIDGET_INPUT) continue;
        if (!w->focused) continue;

        if (keyev_is_backspace(keyev)) {
            int len = (int)strlen(w->value);
            if (len > 0) {
                w->value[len - 1] = 0;
                w->cursor_pos = len - 1;
                wm_invalidate_window(st->window_id);
            }
            return;
        }

        int ch = keyev_to_char(keyev);
        if (ch) {
            int len = (int)strlen(w->value);
            if (len < (int)sizeof(w->value) - 1) {
                w->value[len] = (char)ch;
                w->value[len + 1] = 0;
                w->cursor_pos = len + 1;
                wm_invalidate_window(st->window_id);
            }
            return;
        }
    }
}

static void kef_minimal_on_destroy(app_t* self) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st) return;

    printk("[KEFJSON] destroy win=%d\n", st->window_id);
}

const app_vtbl_t g_kef_minimal_vtbl = {
    .on_create = kef_minimal_on_create,
    .on_destroy = kef_minimal_on_destroy,
    .on_mouse = kef_minimal_on_mouse,
    .on_key = kef_minimal_on_key,
    .on_update = 0,
    .on_draw = kef_minimal_on_draw,
    .on_close_request = 0,
    .on_wheel = 0,
    .tabs_count = 0,
    .tabs_title = 0,
    .tabs_active = 0,
    .tabs_set_active = 0,
};