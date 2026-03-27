#include <ui/ui_label.h>
#include <kernel/drivers/video/gfx.h>

void ui_label_draw(const ui_label_t* l) {
    if (!l || !l->text) return;
    gfx_draw_text_utf8(l->x, l->y, l->color, l->text);
}