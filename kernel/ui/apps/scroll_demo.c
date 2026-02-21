// kernel/ui/apps/scroll_demo.c

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>

#include <ui/apps/scroll_demo.h>
#include <kernel/printk.h>

static scroll_demo_t* sd(app_t* app) {
    return (app && app->user) ? (scroll_demo_t*)app->user : 0;
}

static void recalc(scroll_demo_t* s, int w, int h) {
    if (!s) return;
    s->line_h = 14;
    s->cols = (w > 0) ? (w / 8) : 80;
    if (s->cols < 10) s->cols = 10;
    s->rows = (h > 0) ? (h / s->line_h) : 20;
    if (s->rows < 1) s->rows = 1;
}

static void clamp_view(scroll_demo_t* s) {
    if (!s) return;
    if (s->view_start < 0) s->view_start = 0;

    int max_start = s->line_count - s->rows;
    if (max_start < 0) max_start = 0;
    if (s->view_start > max_start) s->view_start = max_start;
}

static void scroll_by(scroll_demo_t* s, int delta) {
    if (!s) return;
    s->view_start += delta;
    clamp_view(s);
}

typedef struct {
    int x, y, w, h;
    int thumb_y, thumb_h;
    int has_scroll;
} sb_geom_t;

static sb_geom_t sb_calc(scroll_demo_t* s, ui_rect_t client) {
    sb_geom_t g;
    memset(&g, 0, sizeof(g));

    g.w = 10;
    g.x = client.w - g.w - 2;
    g.y = 2;
    g.h = client.h - 4;

    g.has_scroll = (s->line_count > s->rows);

    if (!g.has_scroll) {
        g.thumb_y = g.y;
        g.thumb_h = g.h;
        return g;
    }

    int h = g.h;
    int thumb_h = (h * s->rows) / s->line_count;
    if (thumb_h < 12) thumb_h = 12;
    if (thumb_h > h)  thumb_h = h;

    int max_start = s->line_count - s->rows;
    if (max_start < 1) max_start = 1;

    int thumb_y = g.y + ((h - thumb_h) * s->view_start) / max_start;

    g.thumb_h = thumb_h;
    g.thumb_y = thumb_y;
    return g;
}

static int sb_point_in(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static void draw_scrollbar(scroll_demo_t* s, ui_rect_t client) {
    if (!s) return;

    sb_geom_t g = sb_calc(s, client);
    gfx_fill_rect(g.x, g.y, g.w, g.h, 0x00202020);

    gfx_fill_rect(g.x, g.thumb_y, g.w, g.thumb_h, 0x00606060);
}

static void on_create(app_t* app) {
    scroll_demo_t* s = sd(app);
    if (!s) return;
    memset(s, 0, sizeof(*s));

    // Fill with lines
    s->line_count = SCROLL_DEMO_MAX_LINES;
    for (int i = 0; i < s->line_count; i++) {
        // basit numaralı satır
        // (snprintf yok, basit şekilde)
        char tmp[64];
        int n = i;
        int pos = 0;

        tmp[pos++] = '#';
        tmp[pos++] = ' ';
        // int to string
        char num[16];
        int ni = 0;
        if (n == 0) num[ni++] = '0';
        else {
            int nn = n;
            char rev[16];
            int ri = 0;
            while (nn > 0 && ri < 15) { rev[ri++] = (char)('0' + (nn % 10)); nn /= 10; }
            while (ri > 0) num[ni++] = rev[--ri];
        }
        num[ni] = 0;
        for (int k = 0; num[k] && pos < (int)sizeof(tmp)-1; k++) tmp[pos++] = num[k];

        tmp[pos++] = ' ';
        tmp[pos++] = '-';
        tmp[pos++] = ' ';
        const char* msg = "Scroll demo line (PgUp/PgDn)";
        for (int k = 0; msg[k] && pos < (int)sizeof(tmp)-1; k++) tmp[pos++] = msg[k];
        tmp[pos] = 0;

        strncpy(s->lines[i], tmp, SCROLL_DEMO_LINE_MAX - 1);
        s->lines[i][SCROLL_DEMO_LINE_MAX - 1] = 0;
    }

    s->view_start = 0;
    s->line_h = 14;
    s->cols = 80;
    s->rows = 20;
}

static void on_draw(app_t* app) {
    scroll_demo_t* s = sd(app);
    if (!s) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    gfx_fill_rect(0, 0, client.w, client.h, 0x000000);

    recalc(s, client.w, client.h);
    clamp_view(s);

    int x = 8;
    int y = 8;

    for (int row = 0; row < s->rows; row++) {
        int li = s->view_start + row;
        if (li >= s->line_count) break;

        gfx_draw_text_utf8(x, y, 0x00FF00, s->lines[li]);
        y += s->line_h;
        if (y > client.h - 16) break;
    }

    draw_scrollbar(s, client);
}

static void sb_set_view_from_thumb(scroll_demo_t* s, ui_rect_t client, int thumb_top_y) {
    if (!s) return;

    sb_geom_t g = sb_calc(s, client);
    if (!g.has_scroll) { s->view_start = 0; return; }

    int max_start = s->line_count - s->rows;
    if (max_start < 0) max_start = 0;

    int track_range = g.h - g.thumb_h;
    if (track_range < 1) track_range = 1;

    int rel = thumb_top_y - g.y;
    if (rel < 0) rel = 0;
    if (rel > track_range) rel = track_range;

    // rel -> view_start
    s->view_start = (rel * max_start) / track_range;
    clamp_view(s);
}

static int in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static void on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    scroll_demo_t* s = sd(app);
    if (!s) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);
    sb_geom_t g = sb_calc(s, client);
    if (!g.has_scroll) return;

    // sol click bit: 0x01
    int l_down = (buttons & 0x01) != 0;

    // Track/Thumb alanı (client origin zaten 0,0 geliyor çünkü gfx_set_origin yapıyorsun)
    int tx = s->sb_track_x;
    int ty = s->sb_track_y;
    int tw = s->sb_track_w;
    int th = s->sb_track_h;

    // Eğer scrollbar yoksa çık
    if (tw <= 0 || th <= 0) return;

    // Release -> drag bitir
    if (released & 0x01) {
        s->sb_dragging = 0;
        return;
    }

    // Press -> thumb üstüne bastı mı?
    if (pressed & 0x01) {
        printk("[scroll_demo] click mx=%d my=%d\n", mx, my);
        int thumb_x = tx;
        int thumb_y = s->sb_thumb_y;
        int thumb_w = tw;
        int thumb_h = s->sb_thumb_h;

        if (in_rect(mx, my, thumb_x, thumb_y, thumb_w, thumb_h)) {
            s->sb_dragging = 1;
            s->sb_grab_off = my - thumb_y;
            return;
        }

        // Track'e bastıysa: sayfa sayfa kaydır (thumb üstünde değil)
        if (in_rect(mx, my, tx, ty, tw, th)) {
            if (my < s->sb_thumb_y) scroll_by(s, -s->rows); // page up
            else if (my > s->sb_thumb_y + s->sb_thumb_h) scroll_by(s, +s->rows); // page down
            return;
        }
    }

    // Dragging -> view_start hesapla
    if (s->sb_dragging && l_down) {
        if (s->line_count <= s->rows) { s->view_start = 0; return; }

        int max_start = s->line_count - s->rows;
        if (max_start < 1) max_start = 1;

        int track_range = s->sb_track_h - s->sb_thumb_h;
        if (track_range < 1) track_range = 1;

        int new_thumb_y = my - s->sb_grab_off;
        if (new_thumb_y < s->sb_track_y) new_thumb_y = s->sb_track_y;
        if (new_thumb_y > s->sb_track_y + track_range) new_thumb_y = s->sb_track_y + track_range;

        int rel = new_thumb_y - s->sb_track_y;
        s->view_start = (rel * max_start) / track_range;
        clamp_view(s);
    }
}

static void on_key(app_t* app, uint16_t key) {
    scroll_demo_t* s = sd(app);
    if (!s) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return;

    // PageUp/PageDown
    if (sc == 0x49) { scroll_by(s, -3); return; }
    if (sc == 0x51) { scroll_by(s, +3); return; }

    // Up/Down (Set1: 0x48/0x50 genelde, ama bazen E0 prefix olur)
    if (sc == 0x48) { scroll_by(s, -1); return; }
    if (sc == 0x50) { scroll_by(s, +1); return; }
}

static void on_destroy(app_t* app) { (void)app; }

const app_vtbl_t scroll_demo_vtbl = {
    .on_create  = on_create,
    .on_draw    = on_draw,
    .on_mouse   = on_mouse,
    .on_key     = on_key,
    .on_destroy = on_destroy
};