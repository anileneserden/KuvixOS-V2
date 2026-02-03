#include <ui/widgets/progressbar.h>

// fb_draw_rect buradan geliyor
#include <kernel/drivers/video/fb.h>

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void progressbar_init(progressbar_t* p, int x, int y, int w, int h) {
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;

    p->value = 0;
    p->visible = 1;

    // Default renkler (istersen theme'den set edersin)
    p->bg   = 0xFF2A2A2A;
    p->fill = 0xFF2ECC71;
}

void progressbar_set_value(progressbar_t* p, int value_0_100) {
    p->value = clampi(value_0_100, 0, 100);
}

void progressbar_set_colors(progressbar_t* p, uint32_t bg, uint32_t fill) {
    p->bg = bg;
    p->fill = fill;
}

void progressbar_draw(const progressbar_t* p) {
    if (!p || !p->visible) return;
    if (p->w <= 0 || p->h <= 0) return;

    // Background
    fb_draw_rect(p->x, p->y, p->w, p->h, p->bg);

    // Fill width
    int fill_w = (p->w * p->value) / 100;
    fill_w = clampi(fill_w, 0, p->w);

    // Fill
    if (fill_w > 0) {
        fb_draw_rect(p->x, p->y, fill_w, p->h, p->fill);
    }
}
