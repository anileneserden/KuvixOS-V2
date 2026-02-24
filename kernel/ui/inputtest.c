#include <ui/inputtest.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h>

#include <lib/string.h>
#include <stdint.h>

/* -------------------------------------------------- */
/* STATE                                             */
/* -------------------------------------------------- */

static int g_mx = 100;
static int g_my = 80;

static int g_last_dx = 0;
static int g_last_dy = 0;
static int g_last_wheel = 0;
static uint8_t g_last_buttons = 0;

static uint32_t g_event_count = 0;

/* keyboard debug */
static uint16_t g_last_sc = 0;
static char     g_last_ch = 0;

static int g_shift = 0;
static int g_ctrl  = 0;
static int g_alt   = 0;
static int g_altgr = 0;
static int g_caps  = 0;

/* -------------------------------------------------- */
/* SMALL DRAW HELPERS                                */
/* -------------------------------------------------- */

static void draw_text(int x, int y, const char* s) {
    gfx_draw_text_utf8(x, y, 0x00FFFFFF, s);
}

static void draw_num(int x, int y, const char* label, int v) {
    char buf[64];
    int p = 0;

    while (label && label[p] && p < 40) { buf[p] = label[p]; p++; }
    buf[p++] = ':';
    buf[p++] = ' ';

    char tmp[16];
    int ti = 0;

    int neg = 0;
    if (v < 0) { neg = 1; v = -v; }

    if (v == 0) tmp[ti++] = '0';
    else {
        char rev[16];
        int ri = 0;
        while (v > 0 && ri < 15) { rev[ri++] = (char)('0' + (v % 10)); v /= 10; }
        if (neg) tmp[ti++] = '-';
        while (ri > 0) tmp[ti++] = rev[--ri];
    }

    tmp[ti] = 0;

    for (int i = 0; tmp[i]; i++)
        buf[p++] = tmp[i];

    buf[p] = 0;

    draw_text(x, y, buf);
}

static void draw_hex8(int x, int y, const char* label, uint8_t v) {
    char buf[32];
    const char* hx = "0123456789ABCDEF";

    buf[0] = label[0];
    buf[1] = label[1];
    buf[2] = ':';
    buf[3] = ' ';
    buf[4] = '0';
    buf[5] = 'x';
    buf[6] = hx[(v >> 4) & 0xF];
    buf[7] = hx[v & 0xF];
    buf[8] = 0;

    draw_text(x, y, buf);
}

/* -------------------------------------------------- */
/* INIT                                              */
/* -------------------------------------------------- */

void inputtest_init(void) {
    gfx_clear(0x000000);
    fb_present();
}

/* -------------------------------------------------- */
/* KEYBOARD HANDLER                                  */
/* -------------------------------------------------- */

extern char kbd_scancode_to_ascii(uint8_t sc);

void inputtest_handle_scancode(uint16_t ev)
{
    uint8_t sc  = (uint8_t)(ev & 0xFF);
    uint8_t ext = (uint8_t)(ev >> 8);

    g_last_sc = ev;

    int is_break = (sc & 0x80) != 0;
    uint8_t make = sc & 0x7F;

    // --- Caps toggle (Sadece make)
    if (!is_break && make == 0x3A) {
        g_caps = !g_caps;
        return;
    }

    // --- Shift (L=2A, R=36)
    if (make == 0x2A || make == 0x36) {
        g_shift = !is_break;
        return;
    }

    // --- Ctrl (L=1D, R=E0 1D)
    if (make == 0x1D) {
        g_ctrl = !is_break;
        return;
    }

    // --- Alt / AltGr
    if (make == 0x38) {
        if (ext == 0xE0) g_altgr = !is_break;  // Right Alt = AltGr
        else             g_alt   = !is_break;  // Left Alt
        return;
    }

    // diğer tuşlar: ASCII üret
    if (!is_break) {
        g_last_ch = kbd_scancode_to_ascii(sc);
    }
}

/* -------------------------------------------------- */
/* TICK                                              */
/* -------------------------------------------------- */

void inputtest_tick(void)
{
    int dx, dy, wheel;
    uint8_t buttons;

    while (ps2_mouse_pop(&dx, &dy, &wheel, &buttons)) {

        g_last_dx = dx;
        g_last_dy = dy;
        g_last_wheel = wheel;
        g_last_buttons = buttons;

        g_mx += dx;
        g_my += dy;

        if (g_mx < 0) g_mx = 0;
        if (g_my < 0) g_my = 0;

        int maxx = (int)fb_get_width()  - 1;
        int maxy = (int)fb_get_height() - 1;

        if (g_mx > maxx) g_mx = maxx;
        if (g_my > maxy) g_my = maxy;

        g_event_count++;
    }

    inputtest_draw();
}

/* -------------------------------------------------- */
/* DRAW                                              */
/* -------------------------------------------------- */

void inputtest_draw(void)
{
    gfx_clear(0x000000);

    draw_text(8, 8,  "KuvixOS Input Test");
    draw_text(8, 24, "Mouse + Keyboard debug");

    draw_num(8, 60,  "mx", g_mx);
    draw_num(8, 76,  "my", g_my);
    draw_num(8, 92,  "dx", g_last_dx);
    draw_num(8, 108, "dy", g_last_dy);
    draw_num(8, 124, "wheel", g_last_wheel);

    draw_hex8(8, 150, "sc", (uint8_t)(g_last_sc & 0xFF));

    draw_num(8, 170, "shift", g_shift);
    draw_num(8, 186, "ctrl",  g_ctrl);
    draw_num(8, 202, "alt",   g_alt);
    draw_num(8, 218, "altgr", g_altgr);
    draw_num(8, 234, "caps",  g_caps);

    char cbuf[8];
    cbuf[0] = 'c';
    cbuf[1] = 'h';
    cbuf[2] = ':';
    cbuf[3] = ' ';
    cbuf[4] = (g_last_ch >= 32) ? g_last_ch : '.';
    cbuf[5] = 0;

    draw_text(8, 254, cbuf);

    /* cursor */
    for (int yy = -2; yy <= 2; yy++)
        for (int xx = -2; xx <= 2; xx++)
            gfx_putpixel(g_mx + xx, g_my + yy, 0x00FF00);

    fb_present();
}