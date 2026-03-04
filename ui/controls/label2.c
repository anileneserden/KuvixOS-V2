#include <ui/controls/label2.h>
#include <kernel/drivers/video/gfx.h>

static void label2_draw(ui_control_t* c) {
    ui_label2_t* l = (ui_label2_t*)c;
    // UTF-8 destekli çiz
    gfx_draw_text_utf8(l->base.location.x, l->base.location.y, l->color, l->text ? l->text : "");
}

static bool label2_event(ui_control_t* c, const ui_event_t* e) {
    (void)c; (void)e;
    return false;
}

static const ui_control_vtbl_t label2_vtbl = {
    .draw = label2_draw,
    .handle_event = label2_event
};

void ui_label2_init(ui_label2_t* l, int id, ui_point_t loc, uint32_t color, const char* text) {
    ui_control_init(&l->base, id, loc, (ui_size_t){0,0}, &label2_vtbl);
    l->color = color;
    l->text = text;
}

void ui_label2_set_text(ui_label2_t* l, const char* text) {
    l->text = text;
}
