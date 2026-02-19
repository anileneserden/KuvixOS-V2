#include <ui/controls/button2.h>
#include <kernel/drivers/video/gfx.h>

static void button2_draw(ui_control_t* c) {
    ui_button2_t* b = (ui_button2_t*)c;

    int x = b->base.location.x;
    int y = b->base.location.y;
    int w = b->base.size.w;
    int h = b->base.size.h;

    // basit tema: hover/pressed ile renk değiştir
    uint32_t bg = 0xE0E0E0;
    uint32_t border = 0x404040;
    uint32_t text = 0x000000;

    if (b->base.hovered) bg = 0xD0D0D0;
    if (b->base.pressed) bg = 0xB8B8B8;

    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, border);

    // text’i ortalamaya çok girmeyelim (şimdilik)
    int tx = x + 6;
    int ty = y + (h > 16 ? (h - 16) / 2 : 0);
    gfx_draw_text_utf8(tx, ty, text, b->label ? b->label : "");
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
