#include <ui/mouse.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>

mouse_t g_mouse;

void mouse_init(uint32_t screen_w, uint32_t screen_h) {
    g_mouse.screen_w = screen_w;
    g_mouse.screen_h = screen_h;
    g_mouse.x = screen_w / 2;
    g_mouse.y = screen_h / 2;
    g_mouse.visible = 1;
}

void mouse_move(int dx, int dy) {
    g_mouse.x += dx;
    g_mouse.y += dy;
    if (g_mouse.x < 0) g_mouse.x = 0;
    if (g_mouse.y < 0) g_mouse.y = 0;
    if (g_mouse.x >= (int)g_mouse.screen_w) g_mouse.x = g_mouse.screen_w - 1;
    if (g_mouse.y >= (int)g_mouse.screen_h) g_mouse.y = g_mouse.screen_h - 1;
}

void mouse_draw(void) {
    if (!g_mouse.visible) return;
    // Basit bir ok/imleç çizimi (cursor.h yoksa bile çalışır)
    for(int i=0; i<10; i++) {
        fb_putpixel(g_mouse.x + i, g_mouse.y + i, 0xFFFFFFFF);
        fb_putpixel(g_mouse.x, g_mouse.y + i, 0xFFFFFFFF);
        fb_putpixel(g_mouse.x + i, g_mouse.y, 0xFFFFFFFF);
    }
}