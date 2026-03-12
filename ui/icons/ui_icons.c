#include <ui/bitmaps/icons/icon_close_16.h>
#include <ui/bitmaps/icons/icon_max_16.h>
#include <ui/bitmaps/icons/icon_min_16.h>
#include <kernel/printk.h>

static int g_icons_ready = 0;

void ui_icons_init(void)
{
    if (g_icons_ready) return;
    g_icons_ready = 1;

    icon_close_16_init();
    icon_max_16_init();
    icon_min_16_init();
}