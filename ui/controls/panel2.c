// ui/controls/panel2.c
#include <ui/controls/panel2.h>
#include <kernel/drivers/video/gfx.h>

static void panel2_draw(ui_control_t* c);
static bool panel2_handle(ui_control_t* c, const ui_event_t* e);

static const ui_control_vtbl_t g_panel2_vtbl = {
    .draw = panel2_draw,
    .handle_event = panel2_handle
};

void ui_panel2_init(ui_panel2_t* p,
                    int id,
                    ui_point_t loc,
                    ui_size_t size,
                    uint32_t bg_color)
{
    if (!p) return;
    ui_control_init(&p->base, id, loc, size, &g_panel2_vtbl);
    p->bg_color = bg_color;
    p->draw_border = 0;
    p->border_color = 0x000000;
}

void ui_panel2_set_border(ui_panel2_t* p, int enable, uint32_t border_color) {
    if (!p) return;
    p->draw_border = enable ? 1 : 0;
    p->border_color = border_color;
}

static void draw_children(ui_control_t* c) {
    // children zinciri
    for (ui_control_t* ch = c->first_child; ch; ch = ch->next_sibling) {
        if (!ch->visible) continue;
        if (ch->vtbl && ch->vtbl->draw) ch->vtbl->draw(ch);
    }
}

static void panel2_draw(ui_control_t* c) {
    ui_panel2_t* p = (ui_panel2_t*)c;
    if (!p || !c->visible) return;

    // panel background
    gfx_fill_rect(c->location.x, c->location.y, c->size.w, c->size.h, p->bg_color);

    if (p->draw_border) {
        gfx_draw_rect(c->location.x, c->location.y, c->size.w, c->size.h, p->border_color);
    }

    draw_children(c);
}

static bool panel2_handle(ui_control_t* c, const ui_event_t* e) {
    (void)c; (void)e;
    // panel olayları tüketmez (şimdilik)
    return false;
}
