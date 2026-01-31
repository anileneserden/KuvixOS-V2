// src/lib/themes/theme_bootstrap.c
#include <ui/theme.h>

static int g_bootstrap_done = 0;

void ui_theme_bootstrap_default(void)
{
    if (g_bootstrap_done) return;
    g_bootstrap_done = 1;

    // main branch: sadece built-in theme
    ui_set_theme(ui_get_builtin_theme());

    // FS disabled: default.kth toyfs'ten okunmayacak
}
