#include <ui/inputtest.h>
#include <ui/session.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <lib/string.h>
#include <stdint.h>

static int g_mx = 100;
static int g_my = 80;

static int g_last_dx = 0;
static int g_last_dy = 0;
static int g_last_wheel = 0;
static uint8_t g_last_buttons = 0;

static uint32_t g_event_count = 0;

static uint8_t g_last_sc = 0;
static uint8_t g_shift = 0;
static uint8_t g_ctrl  = 0;
static uint8_t g_alt   = 0;
static uint8_t g_altgr = 0;   // RightAlt (E0 38) ise
static uint8_t g_caps  = 0;

static char g_last_char[8] = "";     // UTF-8 için küçük buffer
static char g_mods_str[64] = "";

static void draw_num(int x, int y, const char* label, int v) {
    char buf[64];
    int p = 0;

    // label
    while (label && label[p] && p < 40) { buf[p] = label[p]; p++; }

    // ": "
    if (p < (int)sizeof(buf)-1) buf[p++] = ':';
    if (p < (int)sizeof(buf)-1) buf[p++] = ' ';

    // int -> string (basit)
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

    // u8 -> decimal
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
    // Başlangıç ekranı
    gfx_clear(0x000000);
    fb_present();
}

void inputtest_tick(void) {
    // Mouse queue’dan çek
    int dx, dy, wheel;
    uint8_t buttons;

    while (ps2_mouse_pop(&dx, &dy, &wheel, &buttons)) {
        g_last_dx = dx;
        g_last_dy = dy;
        g_last_wheel = wheel;
        // g_last_buttons = buttons;
        g_event_count++;

        // Kendi cursor koordinatımız
        g_mx += dx;
        g_my += dy;

        if (g_mx < 0) g_mx = 0;
        if (g_my < 0) g_my = 0;
        int maxx = (int)fb_get_width()  - 1;
        int maxy = (int)fb_get_height() - 1;
        if (g_mx > maxx) g_mx = maxx;
        if (g_my > maxy) g_my = maxy;
    }

    // Her tick çiz (en basit)
    inputtest_draw();
}

void inputtest_draw(void) {
    gfx_clear(0x000000);

    gfx_draw_text_utf8(8, 8, 0x00FFFFFF, "KuvixOS Input Test (no WM / no Desktop)");
    gfx_draw_text_utf8(8, 24, 0x00AAAAAA, "Move mouse / click / wheel. Data below:");

    draw_num(8, 48,  "mx", g_mx);
    draw_num(8, 64,  "my", g_my);
    draw_num(8, 80,  "dx", g_last_dx);
    draw_num(8, 96,  "dy", g_last_dy);
    draw_num(8, 112, "wheel", g_last_wheel);
    draw_u8 (8, 128, "buttons", g_last_buttons);
    draw_num(8, 144, "events", (int)g_event_count);
    draw_num(8, 160, "scancode", g_last_sc);

    // basit cursor (5x5)
    for (int yy = -2; yy <= 2; yy++) {
        for (int xx = -2; xx <= 2; xx++) {
            gfx_putpixel(g_mx + xx, g_my + yy, 0x00FF00);
        }
    }

    fb_present();
}

void inputtest_handle_scancode(uint16_t sc) {
    uint8_t code = (uint8_t)(sc & 0xFF);

    g_last_sc = code;

    if (code & 0x80) return;

    // ESC -> Desktop
    /*if (code == 0x01) {
        ui_session_switch(UI_SESSION_DESKTOP);
        return;
    }*/

    // F1 -> TTY1, F2 -> Desktop, F3 -> Input (opsiyon)
    /*if (code == 0x3B) { // F1
        ui_session_switch(UI_SESSION_TTY1);
        return;
    }
    if (code == 0x3C) { // F2
        ui_session_switch(UI_SESSION_DESKTOP);
        return;
    }
    if (code == 0x3D) { // F3
        ui_session_switch(UI_SESSION_INPUT);
        return;
    }*/
}