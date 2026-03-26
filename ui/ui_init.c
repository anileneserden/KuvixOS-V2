#include <ui/ui_init.h>
#include <kernel/printk.h>

#include <ui/icons.h>
#include <ui/theme.h>

static int g_ui_inited = 0;

void ui_init(void) {
    if (g_ui_inited) return;
    g_ui_inited = 1;

    printk("[UI] ui_init\n");

    // Gerekli genel initler
    // ui_icons_init();
    // ui_theme_bootstrap_default();

    // KUI test çizimini BURADA yapma.
}