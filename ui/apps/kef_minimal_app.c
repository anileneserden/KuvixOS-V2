// ui/apps/kef_minimal_app.c
#include <ui/apps/kef_minimal_app.h>

#include <kernel/drivers/input/keyboard.h>
#include <kernel/exec/kef_json.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <ui/wm.h>
#include <ui/color.h>

extern uint32_t g_ticks_ms;

extern void kef_cpp_smoke_test(kef_minimal_state_t* st);
extern void kef_cpp_on_click(kef_minimal_state_t* st, const char* id);

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

static void kef_bind_events(kef_minimal_state_t* st) {
    (void)st;
}

/*
 * keyev masaüstünden ham scancode olarak geliyor.
 * Karakter üretmek için mevcut keyboard layout helper'ını kullanıyoruz.
 */
static int keyev_to_char(uint16_t keyev) {
    uint8_t sc8 = (uint8_t)(keyev & 0xFF);

    if (sc8 & 0x80) return 0;

    char c = kbd_scancode_to_ascii(sc8);
    if (!c) return 0;

    return (int)(unsigned char)c;
}

static int keyev_is_backspace(uint16_t keyev) {
    uint8_t sc8 = (uint8_t)(keyev & 0xFF);
    return sc8 == 0x0E;
}

static void kef_minimal_on_create(app_t* self) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st) return;

    memset(st, 0, sizeof(*st));
    st->window_id = self->win_id;
    st->loaded = 0;

    strcpy(st->title, "KEF Host");
    st->width = 420;
    st->height = 240;
    st->bg_color = 0xE6E6E6;

    wm_set_title(self->win_id, st->title);
    wm_set_window_size(self->win_id, st->width, st->height);
    wm_invalidate_window(self->win_id);
}

static void kef_minimal_on_draw(app_t* self) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st) return;

    ui_rect_t c = wm_get_client_rect(st->window_id);
    gfx_fill_rect(0, 0, c.w, c.h, st->bg_color);

    if (!st->loaded) {
        gfx_draw_text_utf8(12, 12, COLOR_BLACK, "KEF UI yuklenemedi");
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
                gfx_draw_text(w->x + 4, w->y + 6, w->text_color, w->value);
            } else {
                gfx_draw_text_utf8(w->x + 4, w->y + 6, 0x888888, w->placeholder);
            }

            /* blinking caret */
            if (w->focused) {
                if ((g_ticks_ms - w->caret_last_toggle) >= 500) {
                    w->caret_visible = !w->caret_visible;
                    w->caret_last_toggle = g_ticks_ms;
                }

                if (w->caret_visible) {
                    int text_len = (int)strlen(w->value);
                    int caret_x = w->x + 4 + (text_len * 8);
                    int caret_y = w->y + 4;
                    int caret_h = w->h - 8;

                    if (caret_h < 8) caret_h = 8;
                    gfx_fill_rect(caret_x, caret_y, 1, caret_h, 0x000000);
                }

                /* caret blink için yeniden çizim iste */
                wm_invalidate_window(st->window_id);
            }
        }
        else if (w->type == KEF_WIDGET_COMBOBOX) {
            int item_h = 22;

            gfx_fill_rect(w->x, w->y, w->w, w->h, 0xFFFFFFFF);
            gfx_draw_rect(w->x, w->y, w->w, w->h, 0xFF808080);

            const char* txt = "";
            if (w->combo_selected >= 0 && w->combo_selected < w->combo_item_count) {
                txt = w->combo_items[w->combo_selected];
            }

            gfx_draw_text_utf8(w->x + 6, w->y + 6, 0xFF111111, txt);
            gfx_draw_text_utf8(w->x + w->w - 12, w->y + 6, 0xFF111111, "v");

            if (w->combo_open) {
                for (int j = 0; j < w->combo_item_count; j++) {
                    int iy = w->y + w->h + j * item_h;
                    int selected = (j == w->combo_selected);

                    uint32_t bg = selected ? 0xFF0055AA : 0xFFFFFFFF;
                    uint32_t fg = selected ? 0xFFFFFFFF : 0xFF111111;

                    gfx_fill_rect(w->x, iy, w->w, item_h, bg);
                    gfx_draw_rect(w->x, iy, w->w, item_h, 0xFF808080);
                    gfx_draw_text_utf8(w->x + 6, iy + 5, fg, w->combo_items[j]);
                }
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
                w->caret_visible = 0;
            }
        }

        /* input focus ver */
        for (int i = 0; i < st->widget_count; i++) {
            kef_widget_t* w = &st->widgets[i];
            if (w->type == KEF_WIDGET_INPUT && point_in_widget(w, mx, my)) {
                w->focused = 1;
                w->caret_visible = 1;
                w->caret_last_toggle = g_ticks_ms;
                wm_invalidate_window(st->window_id);
            }
        }

        /* combobox handling */
        for (int i = 0; i < st->widget_count; i++) {
            kef_widget_t* w = &st->widgets[i];
            if (w->type != KEF_WIDGET_COMBOBOX) continue;

            int item_h = 22;

            /* ana kutuya tıklanırsa aç/kapat */
            if (point_in_widget(w, mx, my)) {
                w->combo_open = !w->combo_open;
                wm_invalidate_window(st->window_id);
                return;
            }

            /* dropdown açıksa item seçimi */
            if (w->combo_open) {
                int list_x = w->x;
                int list_y = w->y + w->h;
                int list_w = w->w;
                int list_h = w->combo_item_count * item_h;

                if (mx >= list_x && mx < list_x + list_w &&
                    my >= list_y && my < list_y + list_h) {
                    int idx = (my - list_y) / item_h;

                    if (idx >= 0 && idx < w->combo_item_count) {
                        w->combo_selected = idx;
                    }

                    w->combo_open = 0;
                    wm_invalidate_window(st->window_id);
                    return;
                }

                /* açıkken dışarı tıklanırsa kapat */
                w->combo_open = 0;
                wm_invalidate_window(st->window_id);
            }
        }
    }

    /* button handling */
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
                kef_cpp_on_click(st, w->id);
                wm_invalidate_window(st->window_id);
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
                w->caret_visible = 1;
                w->caret_last_toggle = g_ticks_ms;
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
                w->caret_visible = 1;
                w->caret_last_toggle = g_ticks_ms;
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

static void kef_minimal_on_update(app_t* self) {
    kef_minimal_state_t* st = (kef_minimal_state_t*)self->user;
    if (!st || !st->loaded) return;

    for (int i = 0; i < st->widget_count; i++) {
        kef_widget_t* w = &st->widgets[i];
        if (w->type == KEF_WIDGET_INPUT && w->focused) {
            wm_invalidate_window(st->window_id);
            return;
        }
    }
}

const app_vtbl_t g_kef_minimal_vtbl = {
    .on_create = kef_minimal_on_create,
    .on_destroy = kef_minimal_on_destroy,
    .on_mouse = kef_minimal_on_mouse,
    .on_key = kef_minimal_on_key,
    .on_update = kef_minimal_on_update,
    .on_draw = kef_minimal_on_draw,
    .on_close_request = 0,
    .on_wheel = 0,
    .tabs_count = 0,
    .tabs_title = 0,
    .tabs_active = 0,
    .tabs_set_active = 0,
};