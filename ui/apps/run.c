#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/widgets/textbox.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

extern uint32_t g_ticks_ms;

extern char kbd_scancode_to_ascii(uint8_t scancode);

typedef struct {
    int win_id;
    ui_textbox_t tb;
    char buf[96];
} run_t;

static void run_on_create(app_t* self) {
    run_t* r = (run_t*)self->user;
    if (!r) return;
    r->win_id = self->win_id;

    wm_set_active_id(self->win_id);

    // client coords
    ui_rect_t c = wm_get_client_rect(self->win_id);

    // textbox: x,y,w,h (client-relative)
    ui_textbox_init(&r->tb, 12, 34, c.w - 24, 22, r->buf, (int)sizeof(r->buf));
    r->tb.focused = true;
    r->tb.caret_on = true;
    r->tb.last_blink_ms = g_ticks_ms;
}

static void run_on_draw(app_t* self) {
    run_t* r = (run_t*)self->user;
    if (!r) return;

    ui_rect_t c = wm_get_client_rect(self->win_id);

    // background
    gfx_fill_rect(0, 0, c.w, c.h, 0xE6E6E6);

    // label
    gfx_draw_text_utf8(12, 12, 0x000000, "Calıştır:");

    // textbox
    ui_textbox_draw(&r->tb);

    // hint
    gfx_draw_text_utf8(12, 64, 0x555555, "Enter: calıştır  |  Esc: kapat");
}

static void run_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t extra1, uint8_t extra2) {
    (void)extra1; (void)extra2;

    run_t* r = (run_t*)self->user;
    if (!r) return;

    // ✅ screen -> client
    ui_rect_t client = wm_get_client_rect(self->win_id);
    int lx = mx - client.x;
    int ly = my - client.y;

    static uint8_t prev_buttons = 0;
    uint8_t pressed  = (uint8_t)(buttons & ~prev_buttons);
    uint8_t released = (uint8_t)(prev_buttons & ~buttons);
    prev_buttons = buttons;

    ui_textbox_mouse(&r->tb, lx, ly, pressed, released);
}

static void run_execute(const char* s) {
    if (!s || !s[0]) return;

    // Çok basit örnek mapping:
    if (!strcmp(s, "terminal")) { appmgr_start_app(1); return; }
    if (!strcmp(s, "files"))    { appmgr_start_app(2); return; }
    if (!strcmp(s, "notepad"))  { appmgr_start_app(3); return; }
    if (!strcmp(s, "calc"))     { appmgr_start_app(6); return; }
    if (!strcmp(s, "engine"))   { appmgr_start_app(18); return; }
    if (!strcmp(s, "game"))     { appmgr_start_app(18); return; }

    // sayı girerse direkt id
    int id = 0;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') { id = 0; break; }
        id = id * 10 + (*p - '0');
    }
    if (id > 0 && id != 7) { appmgr_start_app(id); return; }

    // bilinmeyen komut -> şimdilik yok
}

static void run_on_key(app_t* self, uint16_t scancode) {
    run_t* r = (run_t*)self->user;
    if (!r) return;

    uint8_t sc = (uint8_t)(scancode & 0xFF);
    char ascii = kbd_scancode_to_ascii(sc);

    // Esc
    if ((sc & 0x80) == 0 && sc == 0x01) {
        wm_close_window(self->win_id);
        return;
    }

    // Enter
    if ((sc & 0x80) == 0 && sc == 0x1C) {
        run_execute(r->buf);
        wm_close_window(self->win_id);
        return;
    }

    ui_textbox_key(&r->tb, scancode, ascii);
}

static void run_on_destroy(app_t* self) { (void)self; }

const app_vtbl_t run_vtbl = {
    .on_create        = run_on_create,
    .on_draw          = run_on_draw,
    .on_key           = run_on_key,
    .on_mouse         = run_on_mouse,
    .on_destroy       = run_on_destroy,
    .on_close_request = 0
};