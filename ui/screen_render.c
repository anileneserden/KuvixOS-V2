#include <ui/screen.h>

#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>

static void ui_render_panel(const ui_panel_t* p) {
    if (!p || !p->used || !p->visible) return;
    if (p->width <= 0 || p->height <= 0) return;

    gfx_fill_rect(p->x, p->y, p->width, p->height, p->background_color);
}

static void ui_render_label(const ui_label_t* l) {
    if (!l || !l->used || !l->visible) return;
    if (!l->text[0]) return;

    gfx_draw_text_utf8(l->x, l->y, l->color, l->text);
}

void ui_screen_render(const ui_screen_t* screen) {
    if (!screen) return;

    fb_clear(screen->background_color);

    if (screen->panel.used) {
        ui_render_panel(&screen->panel);
    }

    if (screen->label.used) {
        ui_render_label(&screen->label);
    }
}