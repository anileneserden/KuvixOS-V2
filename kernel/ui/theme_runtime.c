#include <ui/theme.h>

static ui_theme_t g_active_theme;

const ui_theme_t* ui_get_theme(void)
{
    return &g_active_theme;
}

void ui_set_theme(const ui_theme_t* th)
{
    if (th) g_active_theme = *th;
}
