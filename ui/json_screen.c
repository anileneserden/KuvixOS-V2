#include <ui/json_screen.h>
#include <ui/screen.h>
#include <ui/desktop_icons.h>
#include <ui/desktop_seed.h>

#include <kernel/printk.h>
#include <kernel/drivers/video/fb.h>
#include <stdint.h>

static ui_screen_t g_screen;

void ui_json_screen_init(void) {
    printk("[json_screen] init\n");

    if (!ui_screen_load("/system/ui/desktop.json", &g_screen)) {
        g_screen.background_color = 0x000000;
        g_screen.loaded = 0;
        g_screen.panel.used = 0;
        g_screen.label.used = 0;
        g_screen.desktop_icons.used = 0;
        printk("[json_screen] fallback background\n");
    }

    if (g_screen.desktop_icons.used) {
        desktop_icons_set_style(&g_screen.desktop_icons.style);

        /* varsayılan .ksf kısayollarını üretir */
        /* içinde desktop_icons_init + snap de var */
        desktop_seed_default_shortcuts(false);

        printk("[json_screen] icon count = %d\n", desktop_icons_get_count());
    }
}

void ui_json_screen_tick(void) {
    if (g_screen.loaded) {
        ui_screen_render(&g_screen);
    } else {
        fb_clear(0x000000);
    }

    fb_present();
}

void ui_json_screen_handle_scancode(uint16_t sc) {
    (void)sc;
}