// ui/apps/controls_test.c

#include <ui/apps/controls_test.h>

#include <app/app.h>
#include <ui/wm.h>

#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// ------------------------------------------------------------
// Helpers: safe vtbl calls
// ------------------------------------------------------------
static void ctrl_draw(ui_control_t* c) {
    if (!c || !c->visible) return;
    if (!c->vtbl || !c->vtbl->draw) return;
    c->vtbl->draw(c);
}

static bool ctrl_event(ui_control_t* c, const ui_event_t* e) {
    if (!c || !c->visible || !c->enabled) return false;
    if (!c->vtbl || !c->vtbl->handle_event) return false;
    return c->vtbl->handle_event(c, e);
}

// ------------------------------------------------------------
// State helper
// ------------------------------------------------------------
static controls_test_t* st(app_t* app) {
    return (app && app->user) ? (controls_test_t*)app->user : NULL;
}

// ------------------------------------------------------------
// Button click
// (ui_click2_fn'in imzası: void (*)(void*) varsayıldı)
// ------------------------------------------------------------
static void on_btn_click(void* user) {
    controls_test_t* s = (controls_test_t*)user;
    if (!s) return;

    s->counter++;
    ui_label2_set_text(&s->status, "Button clicked!");
}

// ------------------------------------------------------------
// Init controls
// ------------------------------------------------------------
static void init_controls(controls_test_t* s) {
    // labels
    ui_label2_init(&s->title,  1, (ui_point_t){ 12, 12 }, 0x00FFFFFF, "Controls Test");
    ui_label2_init(&s->status, 2, (ui_point_t){ 12, 32 }, 0x00AAAAAA, "Ready");

    // button
    ui_button2_init(&s->btn_inc, 10, (ui_point_t){ 12, 60 }, (ui_size_t){ 140, 26 }, "Click me");
    s->btn_inc.on_click = on_btn_click;
    s->btn_inc.on_click_user = s;

    // combobox
    ui_combobox2_init(&s->combo, 12, 96, 220, 24);
    s->combo.items[0] = "One";
    s->combo.items[1] = "Two";
    s->combo.items[2] = "Three";
    s->combo.item_count = 3;
    s->combo.selected = 0;

    // textbox
    textbox2_init(&s->tb, 20, (ui_point_t){ 12, 130 }, (ui_size_t){ 280, 26 });
    s->tb.hint = "Type something and press Enter...";
    s->tb.on_enter  = NULL; // enter'i app içinde yakalayıp status güncelleyeceğiz
    s->tb.on_change = NULL;

    // model
    s->counter = 0;
}

// ------------------------------------------------------------
// Dispatch order (combo first, then textbox, then button)
// Not: combo dropdown açıkken üstte kalsın diye önce combo.
// ------------------------------------------------------------
static bool dispatch_to_controls(controls_test_t* s, const ui_event_t* e) {
    if (!s || !e) return false;

    if (ctrl_event(&s->combo.base, e)) return true;
    if (ctrl_event(&s->tb.base, e))    return true;
    if (ctrl_event(&s->btn_inc.base, e)) return true;

    return false;
}

// ------------------------------------------------------------
// vtbl callbacks
// ------------------------------------------------------------
static void ct_on_create(app_t* app) {
    controls_test_t* s = st(app);
    if (!s) return;

    memset(s, 0, sizeof(*s));
    init_controls(s);
}

static void ct_on_draw(app_t* app) {
    controls_test_t* s = st(app);
    if (!s) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);

    gfx_fill_rect(0, 0, client.w, client.h, 0x00101010);

    ctrl_draw(&s->title.base);
    ctrl_draw(&s->status.base);
    ctrl_draw(&s->btn_inc.base);
    ctrl_draw(&s->combo.base);
    ctrl_draw(&s->tb.base);

    // draw counter
    char num[16];
    char line[64];

    // simple itoa (pozitif için yeter)
    int v = s->counter;
    int p = 0;
    char tmp[16];
    int tp = 0;

    if (v == 0) tmp[tp++] = '0';
    while (v > 0 && tp < 15) { tmp[tp++] = (char)('0' + (v % 10)); v /= 10; }
    while (tp > 0 && p < 15) num[p++] = tmp[--tp];
    num[p] = 0;

    line[0] = 0;
    strcat(line, "Counter: ");
    strcat(line, num);

    gfx_draw_text_utf8(170, 66, 0x00FF6A00, line);

    // combobox change detect (poll)
    static int last_sel = -9999;
    if (last_sel == -9999) last_sel = s->combo.selected;

    if (s->combo.selected != last_sel) {
        last_sel = s->combo.selected;
        ui_label2_set_text(&s->status, "Combo changed!");
    }
}

static void ct_on_mouse(app_t* app, int mx, int my,
                        uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)buttons;
    controls_test_t* s = st(app);
    if (!s) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    int lx = mx - client.x;
    int ly = my - client.y;

    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.mouse_x = lx;
    e.mouse_y = ly;

    e.type = UI_EVT_MOUSE_MOVE;
    dispatch_to_controls(s, &e);

    if (pressed & 1) {
        e.type = UI_EVT_MOUSE_DOWN;
        e.mouse_button = 0;
        dispatch_to_controls(s, &e);
    }

    if (released & 1) {
        e.type = UI_EVT_MOUSE_UP;
        e.mouse_button = 0;
        dispatch_to_controls(s, &e);

        // ✅ BUNU ŞİMDİLİK KALDIR:
        // e.type = UI_EVT_CLICK;
        // dispatch_to_controls(s, &e);
    }
}

static void ct_on_key(app_t* app, uint16_t key) {
    controls_test_t* s = st(app);
    if (!s) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return; // break ignore

    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.type = UI_EVT_KEY_DOWN;
    e.key  = (int)sc;

    bool handled = dispatch_to_controls(s, &e);

    // Textbox focused + Enter -> status'a yaz
    if (handled && sc == 0x1C && s->tb.focused) {
        static char msg[320];
        msg[0] = 0;
        strcat(msg, "Entered: ");
        strcat(msg, textbox2_get_text(&s->tb));
        ui_label2_set_text(&s->status, msg);
    }
}

static void ct_on_destroy(app_t* app) {
    (void)app;
}

const app_vtbl_t controls_test_vtbl = {
    .on_create  = ct_on_create,
    .on_draw    = ct_on_draw,
    .on_mouse   = ct_on_mouse,
    .on_key     = ct_on_key,
    .on_destroy = ct_on_destroy
};