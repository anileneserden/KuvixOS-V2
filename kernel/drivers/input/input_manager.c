#include <kernel/drivers/input/input_manager.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/video/fb.h>
#include <stdint.h>

extern int mouse_x;
extern int mouse_y;

static uint8_t g_mouse_buttons = 0;
static int g_dx_sum = 0;
static int g_dy_sum = 0;

uint8_t input_mouse_buttons(void) {
    return g_mouse_buttons;
}

void input_mouse_frame_delta(int* dx, int* dy) {
    if (dx) *dx = g_dx_sum;
    if (dy) *dy = g_dy_sum;
    g_dx_sum = 0;
    g_dy_sum = 0;
}

void input_init(void) {
    ps2_mouse_init();
    // kbd_init(); // klavye hazır olunca açarsın
}

void input_update(void) {
    // Polling driver -> her frame çağrılmalı
    ps2_mouse_poll();

    int dx, dy;
    uint8_t btn;
    while (ps2_mouse_pop(&dx, &dy, &btn)) {
        g_mouse_buttons = btn;

        // Bu frame toplam delta
        g_dx_sum += dx;
        g_dy_sum += dy;

        // Absolute cursor (desktop için)
        mouse_x += dx;
        mouse_y += dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > (int)fb_get_width() - 5)  mouse_x = (int)fb_get_width() - 5;
        if (mouse_y > (int)fb_get_height() - 5) mouse_y = (int)fb_get_height() - 5;
    }
}
