#include <ui/icons.h>

#include <ui/bitmaps/icons/icon_close_16.h>
#include <ui/bitmaps/icons/icon_max_16.h>
#include <ui/bitmaps/icons/icon_min_16.h>

#include <kernel/printk.h>

const uint32_t* ui_icon_close_16(void) { return g_icon_close_16; }
const uint32_t* ui_icon_max_16(void)   { return g_icon_max_16; }
const uint32_t* ui_icon_min_16(void)   { return g_icon_min_16; }


static int g_icons_inited = 0;

void ui_icons_init(void) {
    if (g_icons_inited) return;
    g_icons_inited = 1;

    printk("[UI] icons init\n");
    icon_close_16_init();
    icon_max_16_init();
    icon_min_16_init();

    // debug: ilk pixel ne?
    printk("[UI] close[0]=0x%x close[10]=0x%x\n",
           (unsigned)g_icon_close_16[0],
           (unsigned)g_icon_close_16[10]);
}