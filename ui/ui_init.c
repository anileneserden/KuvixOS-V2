#include <ui/ui_init.h>
#include <kernel/printk.h>

#include <ui/icons.h>
#include <ui/theme.h>

static int g_ui_inited = 0;

void ui_init(void) {
    if (g_ui_inited) return;
    g_ui_inited = 1;

    printk("[UI] ui_init\n");

    // 1) ikonlar (ARGB bufferları doldur)
    ui_icons_init();

    // 2) theme (built-in fallback + /persist/theme.kth)
    ui_theme_bootstrap_default();

    // 3) burada istersen wm_init(), cursor init vs
    // wm_init();  (sende nerede ise)
}