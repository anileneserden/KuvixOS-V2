#include <ui/controls/button2.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/theme/theme.h>

static void button2_draw(ui_control_t* c) {
    ui_button2_t* b = (ui_button2_t*)c;

    int x = b->base.location.x;
    int y = b->base.location.y;
    int w = b->base.size.w;
    int h = b->base.size.h;

    const ui_theme_t* th = ui_get_theme();

    if (b->base.hovered) th->button_hover_bg;
    if (b->base.pressed) th->button_pressed_bg;

    gfx_fill_rect(x, y, w, h, th->button_bg);
    gfx_draw_rect(x, y, w, h, th->button_border);

    // text’i ortalamaya çok girmeyelim (şimdilik)
    int tx = x + 6;
    int ty = y + (h > 16 ? (h - 16) / 2 : 0);
    gfx_draw_text_utf8(tx, ty, th->button_text, b->label ? b->label : "");
}

static bool button2_event(ui_control_t* c, const ui_event_t* e) {
    ui_button2_t* b = (ui_button2_t*)c;

    if (e->type == UI_EVT_CLICK) {
        if (b->on_click) b->on_click(b->on_click_user);
        return true;
    }
    return false;
}

static const ui_control_vtbl_t button2_vtbl = {
    .draw = button2_draw,
    .handle_event = button2_event
};

void ui_button2_init(ui_button2_t* b, int id, ui_point_t loc, ui_size_t size, const char* label) {
    ui_control_init(&b->base, id, loc, size, &button2_vtbl);
    b->label = label;
    b->on_click = 0;
    b->on_click_user = 0;
}

void ui_button2_onclick(ui_button2_t* b, ui_click2_fn fn, void* user) {
    b->on_click = fn;
    b->on_click_user = user;
}
