// kernel/ui/inputtest.c

#include <ui/inputtest.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <lib/string.h>
#include <stdint.h>

static int g_mx = 100;
static int g_my = 80;

static int     g_last_dx = 0;
static int     g_last_dy = 0;
static int     g_last_wheel_step = 0;   // +1 / -1 (son wheel)
static uint8_t g_last_buttons = 0;

static uint8_t  g_prev_buttons = 0;
static uint32_t g_event_count  = 0;

static int g_wheel_total = 0;           // -500..+500

static char g_last_event[64] = "No event yet";

static void set_event(const char* s) {
    int i = 0;
    while (s && s[i] && i < (int)sizeof(g_last_event) - 1) {
        g_last_event[i] = s[i];
        i++;
    }
    g_last_event[i] = 0;
}

static void draw_num(int x, int y, const char* label, int v) {
    char buf[64];
    int p = 0;

    while (label && label[p] && p < 40) { buf[p] = label[p]; p++; }

    if (p < (int)sizeof(buf)-1) buf[p++] = ':';
    if (p < (int)sizeof(buf)-1) buf[p++] = ' ';

    char tmp[16];
    int ti = 0;

    int neg = 0;
    if (v < 0) { neg = 1; v = -v; }

    if (v == 0) tmp[ti++] = '0';
    else {
        char rev[16];
        int ri = 0;
        while (v > 0 && ri < 15) { rev[ri++] = (char)('0' + (v % 10)); v /= 10; }
        if (neg && ti < 15) tmp[ti++] = '-';
        while (ri > 0 && ti < 15) tmp[ti++] = rev[--ri];
    }
    tmp[ti] = 0;

    for (int i = 0; tmp[i] && p < (int)sizeof(buf)-1; i++) buf[p++] = tmp[i];
    buf[p] = 0;

    gfx_draw_text_utf8(x, y, 0x00FFFFFF, buf);
}

static void draw_u8(int x, int y, const char* label, uint8_t v) {
    char buf[64];
    int p = 0;

    while (label && label[p] && p < 40) { buf[p] = label[p]; p++; }
    if (p < (int)sizeof(buf)-1) buf[p++] = ':';
    if (p < (int)sizeof(buf)-1) buf[p++] = ' ';

    int vv = (int)v;
    char rev[8];
    int ri = 0;
    if (vv == 0) rev[ri++] = '0';
    while (vv > 0 && ri < 7) { rev[ri++] = (char)('0' + (vv % 10)); vv /= 10; }
    while (ri > 0 && p < (int)sizeof(buf)-1) buf[p++] = rev[--ri];
    buf[p] = 0;

    gfx_draw_text_utf8(x, y, 0x00FFFFFF, buf);
}

void inputtest_init(void) {
    gfx_clear(0x000000);
    fb_present();

    g_prev_buttons = 0;
    g_last_buttons = 0;

    g_last_dx = 0;
    g_last_dy = 0;
    g_last_wheel_step = 0;

    g_event_count = 0;
    g_wheel_total = 0;

    set_event("No event yet");
}

void inputtest_tick(void) {
    int dx, dy, wheel;
    uint8_t buttons;

    while (ps2_mouse_pop(&dx, &dy, &wheel, &buttons)) {
        g_last_dx = dx;
        g_last_dy = dy;
        g_last_buttons = buttons;
        g_event_count++;

        // Yeni basılan tuşlar (edge)
        uint8_t pressed = (uint8_t)(buttons & (uint8_t)~g_prev_buttons);

        if (pressed) {
            // Yaygın mapping: 0x01=LMB, 0x02=RMB, 0x04=MMB
            if (pressed & 0x01) set_event("[Mouse Event] Sol tus basildi");
            else if (pressed & 0x02) set_event("[Mouse Event] Sag tus basildi");
            else if (pressed & 0x04) set_event("[Mouse Event] Orta tus (wheel click) basildi");
            else set_event("[Mouse Event] Diger tus basildi");
        }

        if (wheel != 0) {
            // Eğer sende yön tersse şunu aç:
            // wheel = -wheel;

            int step = (wheel > 0) ? 1 : -1;   // her wheel hareketi = +-1

            g_last_wheel_step = step;
            g_wheel_total += step;

            // clamp -500..+500
            if (g_wheel_total > 500)  g_wheel_total = 500;
            if (g_wheel_total < -500) g_wheel_total = -500;

            if (step > 0) set_event("[Mouse Event] Wheel +1");
            else          set_event("[Mouse Event] Wheel -1");
        }

        g_prev_buttons = buttons;

        // Cursor hareketini istersen aç:
        // g_mx += dx;
        // g_my += dy;

        if (g_mx < 0) g_mx = 0;
        if (g_my < 0) g_my = 0;
        int maxx = (int)fb_get_width()  - 1;
        int maxy = (int)fb_get_height() - 1;
        if (g_mx > maxx) g_mx = maxx;
        if (g_my > maxy) g_my = maxy;
    }

    inputtest_draw();
}

void inputtest_draw(void) {
    gfx_clear(0x000000);

    gfx_draw_text_utf8(8, 8,  0x00FFFFFF, "KuvixOS Input Test (no WM / no Desktop)");
    gfx_draw_text_utf8(8, 24, 0x00AAAAAA, "Move mouse / click / wheel. Data below:");

    // Son olay
    gfx_draw_text_utf8(8, 40, 0x00FFFF00, g_last_event);

    // Debug değerleri
    draw_num(8,  64,  "dx",         g_last_dx);
    draw_num(8,  80,  "dy",         g_last_dy);
    draw_num(8,  96,  "wheel_step", g_last_wheel_step); // +1 / -1
    draw_num(8,  112, "wheel_total",g_wheel_total);     // -500..+500
    draw_u8 (8,  128, "buttons",    g_last_buttons);
    draw_num(8,  144, "events",     (int)g_event_count);

    // basit cursor (5x5)
    for (int yy = -2; yy <= 2; yy++) {
        for (int xx = -2; xx <= 2; xx++) {
            int px = g_mx + xx;
            int py = g_my + yy;
            if (px >= 0 && py >= 0 && px < (int)fb_get_width() && py < (int)fb_get_height()) {
                gfx_putpixel(px, py, 0x0000FF00);
            }
        }
    }

    fb_present();
}